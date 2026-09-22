# Кто меняет Smartphone Integration на Apple CarPlay в главном меню

Проверено 2026-09-13 по штатной декомпилированной Java MU1316.
Исходники: `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-vf/`.
Runtime-код проекта не менялся; текущий timing на машине не измерялся.

Ниже описана штатная реализация. Эксперимент с HU-лоадером и подменой
состояния меню удалён 2026-09-14; эти модели снова полностью принадлежат stock Java.

## Владелец состояния

Штатный `TMDeviceControl.SM.BaseState.DiscoveredState.Busy.SubsystemActive.Active`
при входе вызывает `setHMIDeviceState(true)`, при выходе — `setHMIDeviceState(false)`.
Внутри `updateMainWizardModel()` записывает модель
`ACTIVE_SMARTPHONE_DEVICE_CHOICE` (**4239**).

| 4239 | Вариант меню | Widget ID / значение выбора 138 | Label ID |
| --- | --- | --- | --- |
| 0 | Smartphone Integration | 18 | 3197 |
| 1 | Apple CarPlay | 16 | 3199 |
| 2 | Android Auto | 17 | 3198 |
| 3 | CarLife | 19 | 4068 |

В `setHMIDeviceState()` CarPlay определяется через `tmDevice.isCarplayDevice()`.
На отключении модель возвращается в 0. Если модель текущего выбора меню 138
(`MAIN_APPLICATION_WIZARD_CURRENT_APPLICATION_CHOICE`) уже указывает на один из
четырёх smartphone-пунктов, она также переключается на соответствующий новый
Widget ID. Это сохраняет выбор при замене пункта; другие выбранные пункты не трогает.

Отдельно `updateTabbarModel()` пишет модель **4552**
`SMARTPHONE_TABBAR_DEVICE_TYPE_CHOICE`: 1 для CarPlay, 2 для остальных активных
smartphone-систем, 0 при деактивации. Её нельзя путать с моделью главного меню 4239.

## Событие, запускающее замену

```text
DSISmartphoneIntegrationListener.updateDeviceConnectionState(deviceID, state, method, valid)
  -> SmartphoneIntegrationDSILifecycleController: проверка valid, dispatcher
  -> DeviceManager.SMIControllerListener.updateDeviceConnectionState()
     выбирает TMDeviceControl по адресу устройства и connectionMethod
  -> TMDeviceControl.setConnectionStateByDSIState(2)
     CONNECTIONSTATE_STARTED = 2 -> внутреннее сообщение 6
  -> ActivatingSubsystem.connectionStateStarted()
  -> переход в SubsystemActive.Active
  -> Active.enter(): serviceStarted=true, ConnectionState.ACTIVE,
     SPIServiceState.STARTED, setHMIDeviceState(true)
  -> updateMainWizardModel(): 4239 = 1 для CarPlay
```

`ACTIVATING` возникает раньше, в Connecting/ActivatingSubsystem; сам по себе он
ещё не включает значок CarPlay. В ActivatingSubsystem есть timeout 10000 ms,
отменяемый при выходе из этого состояния: это предел ожидания, не обязательная пауза.

## Как меняются надпись и изображение

`SystemScreenBag2.mAINWIZARD()` создаёт четыре отдельных `MenuItemController`.
Все отправляют один event **1959**, но имеют разные Label ID и Widget ID.
При обновлении 4239 `SystemScreenFactory` меняет их видимость через условия
948 (generic), 713 (CarPlay), 714 (Android Auto), 949 (CarLife).
Условия также учитывают доступность/кодирование соответствующей функции.

`AbstractPlaceholderMenuController.getIconData()` для MainWizard возвращает
Widget ID. `MainWizardRendererHigh.applyProperties()` получает его, выбирает
соответствующую пару icon/decorator из layout и выставляет EAL-свойство `AtlasIndex`.
Надпись берётся по Label ID через штатный text provider.
Для проверенных LayoutMIB2HighQ7 и LayoutMIB2HighScale:
CarPlay widget 16 -> atlas 17; generic widget 18 -> atlas 19; decorator у обоих 12.
Это готовые HMI-ресурсы; телефон не передаёт картинку данного пункта меню.

## Связь с ранним loader

Это отдельная цепочка от `updateMode(SCREEN)` -> `ScreenChange` -> model 4095 ->
TerminalMode screen 3200000 -> context 78. Значок означает, что штатная Java получила
STARTED для устройства и вошла в Active; это не проверка первого кадра 59/stream 110
и не подтверждение активного контекста 78. Для раннего loader можно наблюдать
ACTIVATING/STARTED отдельно, но одна запись 4239=1 только меняет меню и не делает
видео готовым. Порядок реальных событий в текущем подключении требует live trace.

## Точные точки в исходниках

- `org/dsi/ifc/smartphoneintegration/Constants.java:15`: STARTED=2.
- `de/audi/app/terminalmode/dsi/smartphoneintegration/SmartphoneIntegrationDSILifecycleController.java:346`: DSI callback.
- `de/audi/app/terminalmode/device/DeviceManager.java:651`: маршрутизация по устройству.
- `de/audi/app/terminalmode/device/TMDeviceControl.java:231`: DSI state -> сообщение.
- Тот же файл: 902 connectionStateStarted; 982 Active.enter; 1050 setHMIDeviceState;
  1062 updateMainWizardModel; 1113 updateTabbarModel.
- `de/audi/tghu/system/hmi/evohigh/SystemScreenBag2.java:1594`: четыре пункта меню.
- `de/audi/tghu/system/hmi/evohigh/SystemScreenFactory.java:2848`: обработка model 4239;
  1287/1293 и 2225/2237: условия показа.
- `de/esolutions/hmi/widgets/audi/evo/widgets/AbstractPlaceholderMenuController.java:270`: Widget ID -> icon data.
- `de/esolutions/hmi/widgets/audi/evo/high/widgets/MainWizardRendererHigh.java:228`: icon data -> AtlasIndex.
- `de/esolutions/hmi/widgets/audi/evo/high/LayoutMIB2HighQ7.java:923`: таблица atlas.

Связанный разбор: [контекст 78 и запуск основного HMI](MAIN_HMI_CONTEXT_78_STARTUP.md).
