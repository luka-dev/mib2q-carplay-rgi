/*
 * TerminalModeBapCombi$ActiveDeviceStateListener
 *
 * PATCHED CLASS - replaces original in lsd.jxe
 *
 * This inner class handles Terminal Mode device state changes.
 * CarPlay hook injection points:
 *
 * Original class decompiled from mu1316 lsd.jxe, hooks added for CarPlay features.
 */
package de.audi.app.terminalmode.combi;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.device.IActiveDeviceStateListener;
import de.audi.app.terminalmode.device.TMDevice;
import de.audi.app.terminalmode.device.TMDevice$ConnectionState;
import de.audi.atip.interapp.bap.data.TimeStamp;
import de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPCurrentStationInfo;
import de.audi.atip.interapp.combi.bap.audio.data.PlayPosition;
import java.lang.reflect.Field;

final class TerminalModeBapCombi$ActiveDeviceStateListener implements IActiveDeviceStateListener {

   private final TerminalModeBapCombi parent;

   private TerminalModeBapCombi$ActiveDeviceStateListener(TerminalModeBapCombi parent) {
      this.parent = parent;
   }

   // Synthetic constructor for outer class access
   TerminalModeBapCombi$ActiveDeviceStateListener(TerminalModeBapCombi parent, TerminalModeBapCombi$1 dummy) {
      this(parent);
   }

   public void updateActiveDeviceState(TMDevice device) {
      // Handle device activating
      if (device.connectionState().is(TMDevice$ConnectionState.ACTIVATING)) {
         CombiBAPServiceTerminalMode bapService = getBapService();
         if (bapService != null) {
            bapService.updateActiveInfoState(9);
            int sourceType = getBapSourceType(device);
            bapService.updateActiveSource(sourceType, 0, 0, false, false, 0);
            bapService.updateCurrentStation(new CombiBAPCurrentStationInfo());
            bapService.updatePlayPosition(
               PlayPosition.builder()
                  .setTimePosition(TimeStamp.getInstanceFromSeconds(65535))
                  .setTotalPlayTime(TimeStamp.getInstanceFromSeconds(65535))
                  .build()
            );
            bapService.updateActiveInfoState(9);
         }

         /* ========== HOOK 1: CarPlay Activate ========== */
         if (device.isCarplayDevice()) {
            try {
               IContext context = getContext();
               if (context != null) {
                  com.luka.carplay.CarPlayHook.onActivate(context);
               }
            } catch (Throwable t) {
               // ignore - don't crash original flow
            }
         }
         /* ============================================== */
      }

      // Handle device disconnect
      if (device.connectionState().is(TMDevice$ConnectionState.NOT_ATTACHED)) {
         /* ========== HOOK 2: CarPlay Deactivate ========== */
         if (device.isCarplayDevice()) {
            try {
               com.luka.carplay.CarPlayHook.onDeactivate();
            } catch (Throwable t) {
               // ignore - don't crash original flow
            }
         }
         /* ================================================ */
      }
   }

   private int getBapSourceType(TMDevice device) {
      if (device.isCarplayDevice()) {
         return 37;  // CarPlay
      } else if (device.isAndroidAutoDevice()) {
         return 39;  // Android Auto
      } else {
         return 40;  // Other
      }
   }

   private CombiBAPServiceTerminalMode getBapService() {
      try {
         Field field = TerminalModeBapCombi.class.getDeclaredField("bapService");
         field.setAccessible(true);
         return (CombiBAPServiceTerminalMode) field.get(this.parent);
      } catch (Exception e) {
         return null;
      }
   }

   private IContext getContext() {
      try {
         Field field = TerminalModeBapCombi.class.getDeclaredField("context");
         field.setAccessible(true);
         return (IContext) field.get(this.parent);
      } catch (Exception e) {
         return null;
      }
   }
}
