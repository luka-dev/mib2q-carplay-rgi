package com.sq5.aa.input;

import com.luka.carplay.core.FrameworkRef;
import com.luka.carplay.core.Module;
import com.luka.carplay.core.ScreenModule;
import com.sq5.aa.luka.AaLog;

import java.io.File;

import org.dsi.ifc.keypanel.DSIKeyPanel;
import org.dsi.ifc.keypanel.DSIKeyPanelListener;
import org.osgi.framework.ServiceRegistration;

/**
 * Stage A of the cockpit options menu (aa-luka/steering-wheel-cluster-menu-implementation.md):
 * observe raw DSIKeyPanel key and encoder events next to the stock listeners, change nothing.
 * Owner 2026-09-29: left/right wheel buttons open our menu, the roller zooms. Before assigning
 * anything we need the real keyboard ids, key codes and states on this car, and whether they
 * arrive while the cockpit shows the AA cluster map. Adapted from luka's SteeringWheelInputModule
 * (registration and listener shape only; no marker, no consumption).
 * Log-only, bounded; SD flag sq5_wheel_probe_off skips registration.
 */
public final class AaWheelProbe implements Module {
    private static final int DSI_INSTANCE = 0;
    private static final int ATTR_VALID = 1;
    private static final int MAX_LINES = 600;
    private static final String[] OFF = { "/fs/sda0/sq5_wheel_probe_off", "/net/mmx/fs/sda0/sq5_wheel_probe_off" };

    private static int lines;
    private static volatile boolean off;

    private final Listener listener = new Listener();
    private FrameworkRef.ServiceHandle handle;
    private DSIKeyPanel panel;
    private ServiceRegistration registration;
    private volatile boolean running;
    private boolean missLogged;
    private boolean offLogged;

    public String name() { return "wheel-probe"; }

    static boolean disabled() {
        for (int i = 0; i < OFF.length; i++) {
            try { if (new File(OFF[i]).exists()) return true; } catch (Throwable t) { /* ignore */ }
        }
        return false;
    }

    /** Bounded event log shared with the BAP map-callback observation (ScreenCombiBAPListener). */
    public static void log(String msg) {
        if (off) return;
        synchronized (AaWheelProbe.class) {
            if (lines >= MAX_LINES) return;
            if (++lines == MAX_LINES) msg = msg + " (wheel log limit reached)";
        }
        AaLog.log("wheel " + msg + " " + context());
    }

    static String context() {
        boolean mirror = false, small = false, nav = false;
        try { mirror = ScreenModule.isMirrorActive(); } catch (Throwable t) { /* ignore */ }
        try { small = ScreenModule.isSmallScreenViewArea(); } catch (Throwable t) { /* ignore */ }
        try { nav = ScreenModule.isNavActive(); } catch (Throwable t) { /* ignore */ }
        return "[aaMap=" + mirror + " small=" + small + " nav=" + nav + "]";
    }

    public synchronized boolean start(FrameworkRef fw) {
        if (registration != null && panel != null) return true;
        if (disabled()) {
            off = true;
            if (!offLogged) { offLogged = true; AaLog.log("wheel probe off (sq5_wheel_probe_off)"); }
            return true;
        }
        if (fw == null || !fw.isReady() || fw.serviceManager() == null) return false;
        FrameworkRef.ServiceHandle h = fw.getServiceHandle(DSIKeyPanel.class);
        if (h == null || !(h.service() instanceof DSIKeyPanel)) {
            if (h != null) h.release();
            if (!missLogged) { missLogged = true; AaLog.log("wheel probe: DSIKeyPanel not available yet"); }
            return false;
        }
        DSIKeyPanel p = (DSIKeyPanel)h.service();
        ServiceRegistration r = null;
        try {
            r = fw.serviceManager().registerDSIListener(DSI_INSTANCE, DSIKeyPanelListener.class.getName(), listener);
            p.setNotification(DSIKeyPanel.ATTR_KEY2, listener);
            p.setNotification(DSIKeyPanel.ATTR_ENCODER2, listener);
            handle = h; panel = p; registration = r; running = true;
            AaLog.log("wheel probe ready (log-only, KEY2 + ENCODER2, max " + MAX_LINES + " lines)");
            return true;
        } catch (Throwable t) {
            try { p.clearNotification(DSIKeyPanel.ATTR_KEY2, listener); } catch (Throwable ignored) { }
            try { p.clearNotification(DSIKeyPanel.ATTR_ENCODER2, listener); } catch (Throwable ignored) { }
            if (r != null) { try { fw.serviceManager().unregisterService(r); } catch (Throwable ignored) { } }
            h.release();
            AaLog.log("wheel probe start failed: " + t);
            return true;   /* observation only: never hold up the other modules */
        }
    }

    public synchronized void stop() {
        DSIKeyPanel p = panel; ServiceRegistration r = registration; FrameworkRef.ServiceHandle h = handle;
        running = false; panel = null; registration = null; handle = null;
        if (p != null) {
            try { p.clearNotification(DSIKeyPanel.ATTR_KEY2, listener); } catch (Throwable t) { /* ignore */ }
            try { p.clearNotification(DSIKeyPanel.ATTR_ENCODER2, listener); } catch (Throwable t) { /* ignore */ }
        }
        if (r != null) { try { r.unregister(); } catch (Throwable t) { /* ignore */ } }
        if (h != null) h.release();
    }

    private final class Listener implements DSIKeyPanelListener {
        public void updateKey2(int keyboardId, int keyCode, int keyState, int timeStamp, int validFlag) {
            if (!running || validFlag != ATTR_VALID) return;
            log("key board=" + keyboardId + " code=" + keyCode + " state=" + keyState + " ts=" + timeStamp);
            try { AaClusterMenu.onKey(keyboardId, keyCode, keyState, System.currentTimeMillis()); }
            catch (Throwable t) { AaLog.log("menu: key handler failed: " + t); }
        }

        public void updateEncoder2(int keyboardId, int keyCode, int steps, int subSteps, int validFlag) {
            if (!running || validFlag != ATTR_VALID) return;
            log("encoder board=" + keyboardId + " code=" + keyCode + " steps=" + steps + " sub=" + subSteps);
        }

        public void asyncException(int errorCode, String errorString, int requestType) { }
        public void updateDisplayTurnMechStatus(int state, int validFlag) { }
        public void updateRecognizerLanguage2(int keyboardId, String language, int languageCode, int validFlag) { }
        public void updateRecognizerMode(int keyboardId, int mode, int validFlag) { }
        public void updateCharacterEvent2(int keyboardId, String[] characters, int[] confidence, int validFlag) { }
        public void updateGesture2(int keyboardId, int gesture, int x, boolean flag, int y,
                                   int z, int a, int b, int c, int validFlag) { }
        public void genericSettingResponse(int setting, int value, int result) { }
        public void updateProximity(int keyboardId, int value, int validFlag) { }
        public void updateAdvancedProximity(int a, int b, int c, int d, int e,
                                            int f, int g, int h, int i, int validFlag) { }
        public void lastKey(int keyboardId, int keyCode, int keyState) { }
        public void updateKeyboardType(int keyboardType, int validFlag) { }
        public void updateTouchSensitiveArea(int keyboardId, int x, int y, int width, int height, int validFlag) { }
        public void getVersionInfo(int keyboardId, int component, String version) { }
        public void updateInputPanelReady(int keyboardId, int startupState, int validFlag) { }
        public void getProperty(int keyboardId, int property, int index, int validFlag, byte[] value) { }
    }
}
