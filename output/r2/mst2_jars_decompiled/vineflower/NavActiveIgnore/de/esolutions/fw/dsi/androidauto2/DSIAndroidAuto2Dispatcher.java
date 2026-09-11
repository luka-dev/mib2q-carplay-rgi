package de.esolutions.fw.dsi.androidauto2;

import de.esolutions.fw.comm.core.IReplyService;
import de.esolutions.fw.comm.dsi.androidauto2.DSIAndroidAuto2Reply;
import de.esolutions.fw.comm.dsi.androidauto2.impl.DSIAndroidAuto2ReplyService;
import de.esolutions.fw.dsi.base.AbstractDispatcher;
import java.lang.reflect.Method;
import java.util.Iterator;
import org.dsi.ifc.androidauto2.CallState;
import org.dsi.ifc.androidauto2.DSIAndroidAuto2Listener;
import org.dsi.ifc.androidauto2.PlaybackInfo;
import org.dsi.ifc.androidauto2.TelephonyState;
import org.dsi.ifc.androidauto2.TrackData;
import org.dsi.ifc.global.ResourceLocator;

public class DSIAndroidAuto2Dispatcher extends AbstractDispatcher implements DSIAndroidAuto2Reply {
   private DSIAndroidAuto2ReplyService service = new DSIAndroidAuto2ReplyService(this);
   static Class class$org$dsi$ifc$androidauto2$DSIAndroidAuto2Listener;
   static Class class$java$lang$String;

   public DSIAndroidAuto2Dispatcher(int var1) {
      super(
         var1,
         (class$org$dsi$ifc$androidauto2$DSIAndroidAuto2Listener == null
               ? (class$org$dsi$ifc$androidauto2$DSIAndroidAuto2Listener = class$("org.dsi.ifc.androidauto2.DSIAndroidAuto2Listener"))
               : class$org$dsi$ifc$androidauto2$DSIAndroidAuto2Listener)
            .getName()
      );
   }

   public IReplyService getService() {
      return this.service;
   }

   public void videoFocusRequestNotification(int var1, int var2) {
      Object[] var3 = this.getResponseListenerList();
      if (var3 != null) {
         for (int var4 = 0; var4 < var3.length; var4++) {
            try {
               DSIAndroidAuto2Listener var5 = (DSIAndroidAuto2Listener)var3[var4];
               var5.videoFocusRequestNotification(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      }
   }

   public void videoAvailable(boolean var1, int var2) {
      Object[] var3 = this.getResponseListenerList();
      if (var3 != null) {
         for (int var4 = 0; var4 < var3.length; var4++) {
            try {
               DSIAndroidAuto2Listener var5 = (DSIAndroidAuto2Listener)var3[var4];
               var5.videoAvailable(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      }
   }

   public void audioFocusRequestNotification(int var1, int var2) {
      Object[] var3 = this.getResponseListenerList();
      if (var3 != null) {
         for (int var4 = 0; var4 < var3.length; var4++) {
            try {
               DSIAndroidAuto2Listener var5 = (DSIAndroidAuto2Listener)var3[var4];
               var5.audioFocusRequestNotification(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      }
   }

   public void audioAvailable(int var1, boolean var2, int var3) {
      Object[] var4 = this.getResponseListenerList();
      if (var4 != null) {
         for (int var5 = 0; var5 < var4.length; var5++) {
            try {
               DSIAndroidAuto2Listener var6 = (DSIAndroidAuto2Listener)var4[var5];
               var6.audioAvailable(var1, var2, var3);
            } catch (Exception var7) {
               this.traceException(var7);
            }
         }
      }
   }

   public void voiceSessionNotification(int var1, int var2) {
      Object[] var3 = this.getResponseListenerList();
      if (var3 != null) {
         for (int var4 = 0; var4 < var3.length; var4++) {
            try {
               DSIAndroidAuto2Listener var5 = (DSIAndroidAuto2Listener)var3[var4];
               var5.voiceSessionNotification(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      }
   }

   public void microphoneRequestNotification(int var1, int var2) {
      Object[] var3 = this.getResponseListenerList();
      if (var3 != null) {
         for (int var4 = 0; var4 < var3.length; var4++) {
            try {
               DSIAndroidAuto2Listener var5 = (DSIAndroidAuto2Listener)var3[var4];
               var5.microphoneRequestNotification(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      }
   }

   public void navFocusRequestNotification(int var1, int var2) {
      Object[] var3 = this.getResponseListenerList();
      if (var3 != null) {
         for (int var4 = 0; var4 < var3.length; var4++) {
            try {
               DSIAndroidAuto2Listener var5 = (DSIAndroidAuto2Listener)var3[var4];
               var5.navFocusRequestNotification(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      }
   }

   public void updateCallState(CallState[] var1, int var2) {
      if ((var2 & 128) == 128) {
         var2 ^= 128;
         Iterator var3 = this.getUnconfirmedNotificationListenerIterator(1);

         while (var3.hasNext()) {
            try {
               DSIAndroidAuto2Listener var4 = (DSIAndroidAuto2Listener)var3.next();
               this.confirmNotificationListener(1, var4);
               var4.updateCallState(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      } else {
         Iterator var8 = this.getNotificationListenerIterator(1);

         while (var8.hasNext()) {
            try {
               DSIAndroidAuto2Listener var9 = (DSIAndroidAuto2Listener)var8.next();
               var9.updateCallState(var1, var2);
            } catch (Exception var5) {
               this.traceException(var5);
            }
         }
      }
   }

   public void updateTelephonyState(TelephonyState var1, int var2) {
      if ((var2 & 128) == 128) {
         var2 ^= 128;
         Iterator var3 = this.getUnconfirmedNotificationListenerIterator(2);

         while (var3.hasNext()) {
            try {
               DSIAndroidAuto2Listener var4 = (DSIAndroidAuto2Listener)var3.next();
               this.confirmNotificationListener(2, var4);
               var4.updateTelephonyState(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      } else {
         Iterator var8 = this.getNotificationListenerIterator(2);

         while (var8.hasNext()) {
            try {
               DSIAndroidAuto2Listener var9 = (DSIAndroidAuto2Listener)var8.next();
               var9.updateTelephonyState(var1, var2);
            } catch (Exception var5) {
               this.traceException(var5);
            }
         }
      }
   }

   public void updateNowPlayingData(TrackData var1, int var2) {
      if ((var2 & 128) == 128) {
         var2 ^= 128;
         Iterator var3 = this.getUnconfirmedNotificationListenerIterator(3);

         while (var3.hasNext()) {
            try {
               DSIAndroidAuto2Listener var4 = (DSIAndroidAuto2Listener)var3.next();
               this.confirmNotificationListener(3, var4);
               var4.updateNowPlayingData(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      } else {
         Iterator var8 = this.getNotificationListenerIterator(3);

         while (var8.hasNext()) {
            try {
               DSIAndroidAuto2Listener var9 = (DSIAndroidAuto2Listener)var8.next();
               var9.updateNowPlayingData(var1, var2);
            } catch (Exception var5) {
               this.traceException(var5);
            }
         }
      }
   }

   public void updatePlaybackState(PlaybackInfo var1, int var2) {
      if ((var2 & 128) == 128) {
         var2 ^= 128;
         Iterator var3 = this.getUnconfirmedNotificationListenerIterator(4);

         while (var3.hasNext()) {
            try {
               DSIAndroidAuto2Listener var4 = (DSIAndroidAuto2Listener)var3.next();
               this.confirmNotificationListener(4, var4);
               var4.updatePlaybackState(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      } else {
         Iterator var8 = this.getNotificationListenerIterator(4);

         while (var8.hasNext()) {
            try {
               DSIAndroidAuto2Listener var9 = (DSIAndroidAuto2Listener)var8.next();
               var9.updatePlaybackState(var1, var2);
            } catch (Exception var5) {
               this.traceException(var5);
            }
         }
      }
   }

   public void updatePlayposition(int var1, int var2) {
      if ((var2 & 128) == 128) {
         var2 ^= 128;
         Iterator var3 = this.getUnconfirmedNotificationListenerIterator(5);

         while (var3.hasNext()) {
            try {
               DSIAndroidAuto2Listener var4 = (DSIAndroidAuto2Listener)var3.next();
               this.confirmNotificationListener(5, var4);
               var4.updatePlayposition(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      } else {
         Iterator var8 = this.getNotificationListenerIterator(5);

         while (var8.hasNext()) {
            try {
               DSIAndroidAuto2Listener var9 = (DSIAndroidAuto2Listener)var8.next();
               var9.updatePlayposition(var1, var2);
            } catch (Exception var5) {
               this.traceException(var5);
            }
         }
      }
   }

   public void updateCoverArtUrl(ResourceLocator var1, int var2) {
      if ((var2 & 128) == 128) {
         var2 ^= 128;
         Iterator var3 = this.getUnconfirmedNotificationListenerIterator(6);

         while (var3.hasNext()) {
            try {
               DSIAndroidAuto2Listener var4 = (DSIAndroidAuto2Listener)var3.next();
               this.confirmNotificationListener(6, var4);
               var4.updateCoverArtUrl(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      } else {
         Iterator var8 = this.getNotificationListenerIterator(6);

         while (var8.hasNext()) {
            try {
               DSIAndroidAuto2Listener var9 = (DSIAndroidAuto2Listener)var8.next();
               var9.updateCoverArtUrl(var1, var2);
            } catch (Exception var5) {
               this.traceException(var5);
            }
         }
      }
   }

   public void updateNavigationNextTurnEvent(String var1, int var2, int var3, int var4, int var5, int var6) {
      if ((var6 & 128) == 128) {
         var6 ^= 128;
         Iterator var7 = this.getUnconfirmedNotificationListenerIterator(7);

         while (var7.hasNext()) {
            try {
               DSIAndroidAuto2Listener var8 = (DSIAndroidAuto2Listener)var7.next();
               this.confirmNotificationListener(7, var8);
               var8.updateNavigationNextTurnEvent(var1, var2, var3, var4, var5, var6);
            } catch (Exception var10) {
               this.traceException(var10);
            }
         }
      } else {
         Iterator var12 = this.getNotificationListenerIterator(7);

         while (var12.hasNext()) {
            try {
               DSIAndroidAuto2Listener var13 = (DSIAndroidAuto2Listener)var12.next();
               var13.updateNavigationNextTurnEvent(var1, var2, var3, var4, var5, var6);
            } catch (Exception var9) {
               this.traceException(var9);
            }
         }
      }
   }

   public void updateNavigationNextTurnDistance(int var1, int var2, int var3) {
      if ((var3 & 128) == 128) {
         var3 ^= 128;
         Iterator var4 = this.getUnconfirmedNotificationListenerIterator(8);

         while (var4.hasNext()) {
            try {
               DSIAndroidAuto2Listener var5 = (DSIAndroidAuto2Listener)var4.next();
               this.confirmNotificationListener(8, var5);
               var5.updateNavigationNextTurnDistance(var1, var2, var3);
            } catch (Exception var7) {
               this.traceException(var7);
            }
         }
      } else {
         Iterator var9 = this.getNotificationListenerIterator(8);

         while (var9.hasNext()) {
            try {
               DSIAndroidAuto2Listener var10 = (DSIAndroidAuto2Listener)var9.next();
               var10.updateNavigationNextTurnDistance(var1, var2, var3);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      }
   }

   public void setExternalDestination(double var1, double var3, String var5, String var6, int var7) {
      System.out.println("DSIAndroidAuto2Dispatcher.setExternalDestination(" + var1 + ", " + var3 + ", " + var5 + ", " + var6 + ", " + var7 + ")");
      new Exception("Stack trace").printStackTrace();
      Object[] var8 = this.getResponseListenerList();
      if (var8 != null) {
         for (int var9 = 0; var9 < var8.length; var9++) {
            try {
               DSIAndroidAuto2Listener var10 = (DSIAndroidAuto2Listener)var8[var9];
               var10.setExternalDestination(var1, var3, var5, var6, var7);
            } catch (Exception var11) {
               this.traceException(var11);
            }
         }
      }
   }

   public void bluetoothPairingRequest(String var1, int var2) {
      Object[] var3 = this.getResponseListenerList();
      if (var3 != null) {
         for (int var4 = 0; var4 < var3.length; var4++) {
            try {
               DSIAndroidAuto2Listener var5 = (DSIAndroidAuto2Listener)var3[var4];
               var5.bluetoothPairingRequest(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      }
   }

   public void asyncException(int var1, String var2, int var3) {
      Object[] var4 = this.getResponseListenerList();
      if (var4 != null) {
         for (int var5 = 0; var5 < var4.length; var5++) {
            try {
               DSIAndroidAuto2Listener var6 = (DSIAndroidAuto2Listener)var4[var5];
               var6.asyncException(var1, var2, var3);
            } catch (Exception var7) {
               this.traceException(var7);
            }
         }
      }
   }

   public void yyIndication(String var1, String var2) {
      Object[] var3 = this.getResponseListenerList();
      if (var3 != null) {
         for (int var4 = 0; var4 < var3.length; var4++) {
            try {
               DSIAndroidAuto2Listener var5 = (DSIAndroidAuto2Listener)var3[var4];
               Method var6 = super.getClass()
                  .getMethod(
                     "yyIndication",
                     class$java$lang$String == null ? class$("java.lang.String") : class$java$lang$String,
                     class$java$lang$String == null ? class$("java.lang.String") : class$java$lang$String
                  );
               var6.invoke(var5, var1, var2);
            } catch (Exception var7) {
               this.traceException(var7);
            }
         }
      }
   }

   static Class class$(String var0) {
      try {
         return Class.forName(var0);
      } catch (ClassNotFoundException var2) {
         throw new NoClassDefFoundError(var0);
      }
   }
}
