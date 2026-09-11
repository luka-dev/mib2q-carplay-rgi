package de.esolutions.fw.dsi.androidauto;

import de.esolutions.fw.comm.core.IReplyService;
import de.esolutions.fw.comm.dsi.androidauto.DSIAndroidAutoReply;
import de.esolutions.fw.comm.dsi.androidauto.impl.DSIAndroidAutoReplyService;
import de.esolutions.fw.dsi.base.AbstractDispatcher;
import java.lang.reflect.Method;
import java.util.Iterator;
import org.dsi.ifc.androidauto.AppState;
import org.dsi.ifc.androidauto.AppStateRequest;
import org.dsi.ifc.androidauto.CallState;
import org.dsi.ifc.androidauto.DSIAndroidAutoListener;
import org.dsi.ifc.androidauto.PlaybackInfo;
import org.dsi.ifc.androidauto.Resource;
import org.dsi.ifc.androidauto.ResourceRequest;
import org.dsi.ifc.androidauto.TelephonyState;
import org.dsi.ifc.androidauto.TrackData;
import org.dsi.ifc.global.ResourceLocator;

public class DSIAndroidAutoDispatcher extends AbstractDispatcher implements DSIAndroidAutoReply {
   private DSIAndroidAutoReplyService service = new DSIAndroidAutoReplyService(this);
   static Class class$org$dsi$ifc$androidauto$DSIAndroidAutoListener;
   static Class class$java$lang$String;

   public DSIAndroidAutoDispatcher(int var1) {
      super(
         var1,
         (class$org$dsi$ifc$androidauto$DSIAndroidAutoListener == null
               ? (class$org$dsi$ifc$androidauto$DSIAndroidAutoListener = class$("org.dsi.ifc.androidauto.DSIAndroidAutoListener"))
               : class$org$dsi$ifc$androidauto$DSIAndroidAutoListener)
            .getName()
      );
   }

   public IReplyService getService() {
      return this.service;
   }

   public void setMode(Resource[] var1, AppState[] var2, int var3) {
      Object[] var4 = this.getResponseListenerList();
      if (var4 != null) {
         for (int var5 = 0; var5 < var4.length; var5++) {
            try {
               DSIAndroidAutoListener var6 = (DSIAndroidAutoListener)var4[var5];
               var6.setMode(var1, var2, var3);
            } catch (Exception var7) {
               this.traceException(var7);
            }
         }
      }
   }

   public void requestModeChange(ResourceRequest[] var1, AppStateRequest[] var2, int var3) {
      Object[] var4 = this.getResponseListenerList();
      if (var4 != null) {
         for (int var5 = 0; var5 < var4.length; var5++) {
            try {
               DSIAndroidAutoListener var6 = (DSIAndroidAutoListener)var4[var5];
               var6.requestModeChange(var1, var2, var3);
            } catch (Exception var7) {
               this.traceException(var7);
            }
         }
      }
   }

   public void updateCallState(CallState[] var1, int var2) {
      if ((var2 & 128) == 128) {
         var2 ^= 128;
         Iterator var3 = this.getUnconfirmedNotificationListenerIterator(8);

         while (var3.hasNext()) {
            try {
               DSIAndroidAutoListener var4 = (DSIAndroidAutoListener)var3.next();
               this.confirmNotificationListener(8, var4);
               var4.updateCallState(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      } else {
         Iterator var8 = this.getNotificationListenerIterator(8);

         while (var8.hasNext()) {
            try {
               DSIAndroidAutoListener var9 = (DSIAndroidAutoListener)var8.next();
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
         Iterator var3 = this.getUnconfirmedNotificationListenerIterator(7);

         while (var3.hasNext()) {
            try {
               DSIAndroidAutoListener var4 = (DSIAndroidAutoListener)var3.next();
               this.confirmNotificationListener(7, var4);
               var4.updateTelephonyState(var1, var2);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      } else {
         Iterator var8 = this.getNotificationListenerIterator(7);

         while (var8.hasNext()) {
            try {
               DSIAndroidAutoListener var9 = (DSIAndroidAutoListener)var8.next();
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
               DSIAndroidAutoListener var4 = (DSIAndroidAutoListener)var3.next();
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
               DSIAndroidAutoListener var9 = (DSIAndroidAutoListener)var8.next();
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
               DSIAndroidAutoListener var4 = (DSIAndroidAutoListener)var3.next();
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
               DSIAndroidAutoListener var9 = (DSIAndroidAutoListener)var8.next();
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
               DSIAndroidAutoListener var4 = (DSIAndroidAutoListener)var3.next();
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
               DSIAndroidAutoListener var9 = (DSIAndroidAutoListener)var8.next();
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
               DSIAndroidAutoListener var4 = (DSIAndroidAutoListener)var3.next();
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
               DSIAndroidAutoListener var9 = (DSIAndroidAutoListener)var8.next();
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
         Iterator var7 = this.getUnconfirmedNotificationListenerIterator(11);

         while (var7.hasNext()) {
            try {
               DSIAndroidAutoListener var8 = (DSIAndroidAutoListener)var7.next();
               this.confirmNotificationListener(11, var8);
               var8.updateNavigationNextTurnEvent(var1, var2, var3, var4, var5, var6);
            } catch (Exception var10) {
               this.traceException(var10);
            }
         }
      } else {
         Iterator var12 = this.getNotificationListenerIterator(11);

         while (var12.hasNext()) {
            try {
               DSIAndroidAutoListener var13 = (DSIAndroidAutoListener)var12.next();
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
         Iterator var4 = this.getUnconfirmedNotificationListenerIterator(12);

         while (var4.hasNext()) {
            try {
               DSIAndroidAutoListener var5 = (DSIAndroidAutoListener)var4.next();
               this.confirmNotificationListener(12, var5);
               var5.updateNavigationNextTurnDistance(var1, var2, var3);
            } catch (Exception var7) {
               this.traceException(var7);
            }
         }
      } else {
         Iterator var9 = this.getNotificationListenerIterator(12);

         while (var9.hasNext()) {
            try {
               DSIAndroidAutoListener var10 = (DSIAndroidAutoListener)var9.next();
               var10.updateNavigationNextTurnDistance(var1, var2, var3);
            } catch (Exception var6) {
               this.traceException(var6);
            }
         }
      }
   }

   public void setExternalDestination(double var1, double var3, String var5, String var6) {
      System.out.println("DSIAndroidAutoDispatcher.setExternalDestination(" + var1 + ", " + var3 + ", " + var5 + ", " + var6 + ")");
      new Exception("Stack trace").printStackTrace();
      Object[] var7 = this.getResponseListenerList();
      if (var7 != null) {
         for (int var8 = 0; var8 < var7.length; var8++) {
            try {
               DSIAndroidAutoListener var9 = (DSIAndroidAutoListener)var7[var8];
               var9.setExternalDestination(var1, var3, var5, var6);
            } catch (Exception var10) {
               this.traceException(var10);
            }
         }
      }
   }

   public void asyncException(int var1, String var2, int var3) {
      Object[] var4 = this.getResponseListenerList();
      if (var4 != null) {
         for (int var5 = 0; var5 < var4.length; var5++) {
            try {
               DSIAndroidAutoListener var6 = (DSIAndroidAutoListener)var4[var5];
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
               DSIAndroidAutoListener var5 = (DSIAndroidAutoListener)var3[var4];
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
