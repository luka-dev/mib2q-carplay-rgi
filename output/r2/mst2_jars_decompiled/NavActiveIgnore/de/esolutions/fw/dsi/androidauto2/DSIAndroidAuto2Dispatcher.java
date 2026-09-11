/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  de.esolutions.fw.comm.core.IReplyService
 *  de.esolutions.fw.comm.dsi.androidauto2.DSIAndroidAuto2Reply
 *  de.esolutions.fw.comm.dsi.androidauto2.impl.DSIAndroidAuto2ReplyService
 *  de.esolutions.fw.dsi.base.AbstractDispatcher
 *  org.dsi.ifc.androidauto2.CallState
 *  org.dsi.ifc.androidauto2.DSIAndroidAuto2Listener
 *  org.dsi.ifc.androidauto2.PlaybackInfo
 *  org.dsi.ifc.androidauto2.TelephonyState
 *  org.dsi.ifc.androidauto2.TrackData
 *  org.dsi.ifc.base.DSIListener
 *  org.dsi.ifc.global.ResourceLocator
 */
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
import org.dsi.ifc.base.DSIListener;
import org.dsi.ifc.global.ResourceLocator;

public class DSIAndroidAuto2Dispatcher
extends AbstractDispatcher
implements DSIAndroidAuto2Reply {
    private DSIAndroidAuto2ReplyService service = new DSIAndroidAuto2ReplyService((DSIAndroidAuto2Reply)this);
    static Class class$org$dsi$ifc$androidauto2$DSIAndroidAuto2Listener;
    static Class class$java$lang$String;

    public DSIAndroidAuto2Dispatcher(int n) {
        super(n, (class$org$dsi$ifc$androidauto2$DSIAndroidAuto2Listener == null ? (class$org$dsi$ifc$androidauto2$DSIAndroidAuto2Listener = DSIAndroidAuto2Dispatcher.class$("org.dsi.ifc.androidauto2.DSIAndroidAuto2Listener")) : class$org$dsi$ifc$androidauto2$DSIAndroidAuto2Listener).getName());
    }

    public IReplyService getService() {
        return this.service;
    }

    public void videoFocusRequestNotification(int n, int n2) {
        Object[] objectArray = this.getResponseListenerList();
        if (objectArray != null) {
            for (int i = 0; i < objectArray.length; ++i) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)objectArray[i];
                    dSIAndroidAuto2Listener.videoFocusRequestNotification(n, n2);
                    continue;
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void videoAvailable(boolean bl, int n) {
        Object[] objectArray = this.getResponseListenerList();
        if (objectArray != null) {
            for (int i = 0; i < objectArray.length; ++i) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)objectArray[i];
                    dSIAndroidAuto2Listener.videoAvailable(bl, n);
                    continue;
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void audioFocusRequestNotification(int n, int n2) {
        Object[] objectArray = this.getResponseListenerList();
        if (objectArray != null) {
            for (int i = 0; i < objectArray.length; ++i) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)objectArray[i];
                    dSIAndroidAuto2Listener.audioFocusRequestNotification(n, n2);
                    continue;
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void audioAvailable(int n, boolean bl, int n2) {
        Object[] objectArray = this.getResponseListenerList();
        if (objectArray != null) {
            for (int i = 0; i < objectArray.length; ++i) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)objectArray[i];
                    dSIAndroidAuto2Listener.audioAvailable(n, bl, n2);
                    continue;
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void voiceSessionNotification(int n, int n2) {
        Object[] objectArray = this.getResponseListenerList();
        if (objectArray != null) {
            for (int i = 0; i < objectArray.length; ++i) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)objectArray[i];
                    dSIAndroidAuto2Listener.voiceSessionNotification(n, n2);
                    continue;
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void microphoneRequestNotification(int n, int n2) {
        Object[] objectArray = this.getResponseListenerList();
        if (objectArray != null) {
            for (int i = 0; i < objectArray.length; ++i) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)objectArray[i];
                    dSIAndroidAuto2Listener.microphoneRequestNotification(n, n2);
                    continue;
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void navFocusRequestNotification(int n, int n2) {
        Object[] objectArray = this.getResponseListenerList();
        if (objectArray != null) {
            for (int i = 0; i < objectArray.length; ++i) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)objectArray[i];
                    dSIAndroidAuto2Listener.navFocusRequestNotification(n, n2);
                    continue;
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void updateCallState(CallState[] callStateArray, int n) {
        if ((n & 0x80) == 128) {
            n ^= 0x80;
            Iterator iterator = this.getUnconfirmedNotificationListenerIterator(1);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    this.confirmNotificationListener(1, (DSIListener)dSIAndroidAuto2Listener);
                    dSIAndroidAuto2Listener.updateCallState(callStateArray, n);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        } else {
            Iterator iterator = this.getNotificationListenerIterator(1);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    dSIAndroidAuto2Listener.updateCallState(callStateArray, n);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void updateTelephonyState(TelephonyState telephonyState, int n) {
        if ((n & 0x80) == 128) {
            n ^= 0x80;
            Iterator iterator = this.getUnconfirmedNotificationListenerIterator(2);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    this.confirmNotificationListener(2, (DSIListener)dSIAndroidAuto2Listener);
                    dSIAndroidAuto2Listener.updateTelephonyState(telephonyState, n);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        } else {
            Iterator iterator = this.getNotificationListenerIterator(2);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    dSIAndroidAuto2Listener.updateTelephonyState(telephonyState, n);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void updateNowPlayingData(TrackData trackData, int n) {
        if ((n & 0x80) == 128) {
            n ^= 0x80;
            Iterator iterator = this.getUnconfirmedNotificationListenerIterator(3);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    this.confirmNotificationListener(3, (DSIListener)dSIAndroidAuto2Listener);
                    dSIAndroidAuto2Listener.updateNowPlayingData(trackData, n);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        } else {
            Iterator iterator = this.getNotificationListenerIterator(3);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    dSIAndroidAuto2Listener.updateNowPlayingData(trackData, n);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void updatePlaybackState(PlaybackInfo playbackInfo, int n) {
        if ((n & 0x80) == 128) {
            n ^= 0x80;
            Iterator iterator = this.getUnconfirmedNotificationListenerIterator(4);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    this.confirmNotificationListener(4, (DSIListener)dSIAndroidAuto2Listener);
                    dSIAndroidAuto2Listener.updatePlaybackState(playbackInfo, n);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        } else {
            Iterator iterator = this.getNotificationListenerIterator(4);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    dSIAndroidAuto2Listener.updatePlaybackState(playbackInfo, n);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void updatePlayposition(int n, int n2) {
        if ((n2 & 0x80) == 128) {
            n2 ^= 0x80;
            Iterator iterator = this.getUnconfirmedNotificationListenerIterator(5);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    this.confirmNotificationListener(5, (DSIListener)dSIAndroidAuto2Listener);
                    dSIAndroidAuto2Listener.updatePlayposition(n, n2);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        } else {
            Iterator iterator = this.getNotificationListenerIterator(5);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    dSIAndroidAuto2Listener.updatePlayposition(n, n2);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void updateCoverArtUrl(ResourceLocator resourceLocator, int n) {
        if ((n & 0x80) == 128) {
            n ^= 0x80;
            Iterator iterator = this.getUnconfirmedNotificationListenerIterator(6);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    this.confirmNotificationListener(6, (DSIListener)dSIAndroidAuto2Listener);
                    dSIAndroidAuto2Listener.updateCoverArtUrl(resourceLocator, n);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        } else {
            Iterator iterator = this.getNotificationListenerIterator(6);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    dSIAndroidAuto2Listener.updateCoverArtUrl(resourceLocator, n);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void updateNavigationNextTurnEvent(String string, int n, int n2, int n3, int n4, int n5) {
        if ((n5 & 0x80) == 128) {
            n5 ^= 0x80;
            Iterator iterator = this.getUnconfirmedNotificationListenerIterator(7);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    this.confirmNotificationListener(7, (DSIListener)dSIAndroidAuto2Listener);
                    dSIAndroidAuto2Listener.updateNavigationNextTurnEvent(string, n, n2, n3, n4, n5);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        } else {
            Iterator iterator = this.getNotificationListenerIterator(7);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    dSIAndroidAuto2Listener.updateNavigationNextTurnEvent(string, n, n2, n3, n4, n5);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void updateNavigationNextTurnDistance(int n, int n2, int n3) {
        if ((n3 & 0x80) == 128) {
            n3 ^= 0x80;
            Iterator iterator = this.getUnconfirmedNotificationListenerIterator(8);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    this.confirmNotificationListener(8, (DSIListener)dSIAndroidAuto2Listener);
                    dSIAndroidAuto2Listener.updateNavigationNextTurnDistance(n, n2, n3);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        } else {
            Iterator iterator = this.getNotificationListenerIterator(8);
            while (iterator.hasNext()) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)iterator.next();
                    dSIAndroidAuto2Listener.updateNavigationNextTurnDistance(n, n2, n3);
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void setExternalDestination(double d, double d2, String string, String string2, int n) {
        System.out.println("DSIAndroidAuto2Dispatcher.setExternalDestination(" + d + ", " + d2 + ", " + string + ", " + string2 + ", " + n + ")");
        new Exception("Stack trace").printStackTrace();
        Object[] objectArray = this.getResponseListenerList();
        if (objectArray != null) {
            for (int i = 0; i < objectArray.length; ++i) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)objectArray[i];
                    dSIAndroidAuto2Listener.setExternalDestination(d, d2, string, string2, n);
                    continue;
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void bluetoothPairingRequest(String string, int n) {
        Object[] objectArray = this.getResponseListenerList();
        if (objectArray != null) {
            for (int i = 0; i < objectArray.length; ++i) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)objectArray[i];
                    dSIAndroidAuto2Listener.bluetoothPairingRequest(string, n);
                    continue;
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void asyncException(int n, String string, int n2) {
        Object[] objectArray = this.getResponseListenerList();
        if (objectArray != null) {
            for (int i = 0; i < objectArray.length; ++i) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)objectArray[i];
                    dSIAndroidAuto2Listener.asyncException(n, string, n2);
                    continue;
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    public void yyIndication(String string, String string2) {
        Object[] objectArray = this.getResponseListenerList();
        if (objectArray != null) {
            for (int i = 0; i < objectArray.length; ++i) {
                try {
                    DSIAndroidAuto2Listener dSIAndroidAuto2Listener = (DSIAndroidAuto2Listener)objectArray[i];
                    Method method = super.getClass().getMethod("yyIndication", class$java$lang$String == null ? DSIAndroidAuto2Dispatcher.class$("java.lang.String") : class$java$lang$String, class$java$lang$String == null ? DSIAndroidAuto2Dispatcher.class$("java.lang.String") : class$java$lang$String);
                    method.invoke((Object)dSIAndroidAuto2Listener, string, string2);
                    continue;
                }
                catch (Exception exception) {
                    this.traceException(exception);
                }
            }
        }
    }

    static Class class$(String string) {
        try {
            return Class.forName(string);
        }
        catch (ClassNotFoundException classNotFoundException) {
            throw new NoClassDefFoundError(string);
        }
    }
}

