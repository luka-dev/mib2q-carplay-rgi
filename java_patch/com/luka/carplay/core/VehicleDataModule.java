/*
 * VehicleDataModule -- reads vehicle data the HMI already receives over DSI and hands a
 * snapshot to hook/vehicle (bus CMD_VEHICLE) five times a second while CarPlay is up.
 *
 *   speed           DSICarVehicleStates DynamicVehicleInfoHighFrequent (CAN wheel speed;
 *                   realVehicleSpeed preferred over the displayed vehicleSpeed)
 *   reverse gear    DSIGeneralVehicleStates updateReverseGear
 *   standstill      DSIGeneralVehicleStates updateVehicleStandstill (speed 0 while the car
 *                   stands, in case the speed stream only reports changes)
 *   fuel warning    DSIGeneralVehicleStates updateTankInfo
 *   range           DSICarKombi updateBCTotalCurrentRange, else updateBCCurrentRange1
 *   outside temp    DSICarKombi updateBCOutsideTemperature
 *
 * The hook turns speed and gear into $PASCD sentences when the phone asks for them, and
 * range / temperature / warning into VehicleStatusUpdate.  Whether either is sent is the
 * hook's decision (carplay_startup.sh IAP2_VEHICLE_*); this module only reports.  A value
 * is sent as valid only while DSI says so and, for speed, while it is fresher than 1 s.
 * Each DSI is optional: what is missing on a car is logged once and left out.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.core;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;

import org.dsi.ifc.base.DSIListener;
import org.dsi.ifc.carkombi.DSICarKombi;
import org.dsi.ifc.carkombi.DSICarKombiListener;
import org.dsi.ifc.carvehiclestates.DSICarVehicleStates;
import org.dsi.ifc.carvehiclestates.DSICarVehicleStatesListener;
import org.dsi.ifc.carvehiclestates.DynamicVehicleInfoHighFrequent;
import org.dsi.ifc.carvehiclestates.DynamicVehicleInfoHighFrequentViewOptions;
import org.dsi.ifc.carvehiclestates.DynamicVehicleInfoMidFrequent;
import org.dsi.ifc.carvehiclestates.DynamicVehicleInfoMidFrequentViewOptions;
import org.dsi.ifc.carvehiclestates.DynamicVehicleInfoSCR;
import org.dsi.ifc.carvehiclestates.KeyData;
import org.dsi.ifc.carvehiclestates.OilLevelData;
import org.dsi.ifc.carvehiclestates.SemiStaticDataViewOptions;
import org.dsi.ifc.carvehiclestates.SemiStaticVehicleData;
import org.dsi.ifc.carvehiclestates.VehicleInfoViewOptions;
import org.dsi.ifc.generalvehiclestates.DSIGeneralVehicleStates;
import org.dsi.ifc.generalvehiclestates.DSIGeneralVehicleStatesListener;
import org.dsi.ifc.generalvehiclestates.TankInfo;
import org.dsi.ifc.global.CarBCCurrentRange;
import org.dsi.ifc.global.CarBCDistance;
import org.dsi.ifc.global.CarBCSpeed;
import org.dsi.ifc.global.CarBCTemperature;
import org.dsi.ifc.global.CarViewOption;
import org.osgi.framework.ServiceRegistration;

public final class VehicleDataModule implements Module {
    private static final String TAG = "Vehicle";

    private static final int DSI_INSTANCE = 0;
    private static final int ATTR_VALID = 1;
    private static final int BC_VALID = 1;                  /* CarConstants.BCVALUESTATE_VALID */
    private static final int UNIT_IMPERIAL = 1;             /* MPH, MI, F */
    private static final long PERIOD_MS = 200L;
    private static final long SPEED_FRESH_MS = 1000L;

    /* CarConstants / stock attribute ids. */
    private static final int ATTR_BCCURRENTRANGE1 = 8, ATTR_BCOUTSIDETEMPERATURE = 65,
                             ATTR_BCTOTALCURRENTRANGE = 69;
    private static final int ATTR_TANKINFO = 4, ATTR_REVERSEGEAR = 20, ATTR_VEHICLESTANDSTILL = 22;
    private static final int ATTR_DYNAMICVEHICLEINFOHIGHFREQUENT = 14;

    /* hook/vehicle/vehicle_hook.h */
    static final int VALID_SPEED = 0x01, VALID_GEAR = 0x02, VALID_RANGE = 0x04,
                     VALID_TEMP = 0x08, VALID_WARNING = 0x10;
    static final int GEAR_DRIVE = 1, GEAR_REVERSE = 2;

    /* Latest DSI values, written by DSI threads, read by the sender. */
    private final Object state = new Object();
    private int speedCms = -1;
    private long speedAt;
    private int reverse = -1;                               /* -1 unknown, 0 no, 1 yes */
    private boolean standstill;
    private int rangeKmTotal = -1, rangeKm1 = -1;
    private int tempDc = Integer.MIN_VALUE;
    private int fuelWarning = -1;

    /* Built in start(), never in the constructor: CarPlayApp creates every module in its static
     * initializer, and a class that fails to link there (2026-10-10: a stock adapter missing at
     * runtime) took the whole TerminalMode and Navi bundles down.  Here it only disables this
     * module. */
    private Binding kombi, general, dynamic;
    private boolean linkFailureLogged;
    private volatile boolean tempLogged;

    private Thread sender;
    private volatile boolean running;

    public String name() { return "vehicle-data"; }

    public synchronized boolean start(FrameworkRef fw) {
        if (running) return true;
        if (fw == null || !fw.isReady() || fw.serviceManager() == null) return false;
        try {
            if (kombi == null) {
                kombi = new Binding("DSICarKombi", DSICarKombi.class, DSICarKombiListener.class,
                    new KombiListener(),
                    new int[] { ATTR_BCCURRENTRANGE1, ATTR_BCOUTSIDETEMPERATURE, ATTR_BCTOTALCURRENTRANGE });
                general = new Binding("DSIGeneralVehicleStates", DSIGeneralVehicleStates.class,
                    DSIGeneralVehicleStatesListener.class, new GeneralListener(),
                    new int[] { ATTR_TANKINFO, ATTR_REVERSEGEAR, ATTR_VEHICLESTANDSTILL });
                dynamic = new Binding("DSICarVehicleStates", DSICarVehicleStates.class,
                    DSICarVehicleStatesListener.class, new DynamicListener(),
                    new int[] { ATTR_DYNAMICVEHICLEINFOHIGHFREQUENT });
            }
        } catch (Throwable t) {
            kombi = general = dynamic = null;
            if (!linkFailureLogged) { linkFailureLogged = true; Log.w(TAG, "vehicle data disabled: " + t); }
            return true;                                    /* never block the modules before it */
        }
        int bound = 0;
        if (kombi.bind(fw)) bound++;
        if (general.bind(fw)) bound++;
        if (dynamic.bind(fw)) bound++;
        if (bound == 0) return false;                       /* retried by CarPlayApp */
        running = true;
        sender = new Thread(new Runnable() {
            public void run() { sendLoop(); }
        }, "carplay-vehicle");
        sender.setDaemon(true);
        sender.start();
        Log.i(TAG, "vehicle data: kombi=" + kombi.isBound() + " general=" + general.isBound()
            + " dynamic=" + dynamic.isBound());
        return true;
    }

    public synchronized void stop() {
        running = false;
        Thread t = sender;
        sender = null;
        if (t != null) t.interrupt();
        if (kombi != null) kombi.release();
        if (general != null) general.release();
        if (dynamic != null) dynamic.release();
        synchronized (state) {
            speedCms = -1; reverse = -1; standstill = false; rangeKmTotal = -1; rangeKm1 = -1;
            tempDc = Integer.MIN_VALUE; fuelWarning = -1;
        }
    }

    private void sendLoop() {
        int loggedValid = -1;
        long nextLog = 0;
        while (running) {
            try { Thread.sleep(PERIOD_MS); }
            catch (InterruptedException e) { return; }
            if (!running) return;
            long now = System.currentTimeMillis();
            byte[] p = snapshot(now);
            /* Log what is valid when that changes, else once a minute (speed cm/s, range km, 0.1 C). */
            if (p[1] != loggedValid || now >= nextLog) {
                loggedValid = p[1];
                nextLog = now + 60000L;
                Log.i(TAG, "snapshot valid=0x" + Integer.toHexString(p[1] & 0xFF)
                    + " speed=" + ((p[2] & 0xFF) | (p[3] & 0xFF) << 8) + " gear=" + p[4]
                    + " range=" + ((p[5] & 0xFF) | (p[6] & 0xFF) << 8)
                    + " temp=" + (short)((p[7] & 0xFF) | (p[8] & 0xFF) << 8) + " warn=" + p[9]);
            }
            CarplayBus bus = CarplayBus.getInstance();
            if (bus.isConnected()) bus.sendBinary(CarplayBus.CMD_VEHICLE, p);
        }
    }

    /** CMD_VEHICLE payload (hook/framework/bus_protocol.h), little-endian, 14 bytes. */
    byte[] snapshot(long now) {
        int valid = 0, speed = 0, gear = 0, range = 0, temp = 0, warn = 0;
        synchronized (state) {
            if (speedCms >= 0 && now - speedAt <= SPEED_FRESH_MS) { valid |= VALID_SPEED; speed = speedCms; }
            else if (standstill) valid |= VALID_SPEED;           /* stands: 0 m/s is live data */
            if (reverse >= 0) { valid |= VALID_GEAR; gear = reverse == 1 ? GEAR_REVERSE : GEAR_DRIVE; }
            int r = rangeKmTotal >= 0 ? rangeKmTotal : rangeKm1;
            if (r >= 0) { valid |= VALID_RANGE; range = r; }
            if (tempDc != Integer.MIN_VALUE) { valid |= VALID_TEMP; temp = tempDc; }
            if (fuelWarning >= 0) { valid |= VALID_WARNING; warn = fuelWarning; }
        }
        return encode(valid, speed, gear, range, temp, warn, now);
    }

    static byte[] encode(int valid, int speedCms, int gear, int rangeKm, int tempDc, int warn, long now) {
        byte[] p = new byte[14];
        p[0] = 1;
        p[1] = (byte)valid;
        p[2] = (byte)speedCms; p[3] = (byte)(speedCms >> 8);
        p[4] = (byte)gear;
        p[5] = (byte)rangeKm; p[6] = (byte)(rangeKm >> 8);
        p[7] = (byte)tempDc; p[8] = (byte)(tempDc >> 8);
        p[9] = (byte)warn;
        int ms = (int)now;
        p[10] = (byte)ms; p[11] = (byte)(ms >> 8); p[12] = (byte)(ms >> 16); p[13] = (byte)(ms >> 24);
        return p;
    }

    /* km/h or mph -> cm/s, clamped to the u16 field. */
    static int speedToCms(float value, int unit) {
        double kmh = unit == UNIT_IMPERIAL ? value * 1.609344 : value;
        long cms = Math.round(kmh * 100000.0 / 3600.0);
        return cms < 0 ? 0 : (int)Math.min(cms, 65535L);
    }

    static int distanceToKm(double value, int unit) {
        double km = unit == UNIT_IMPERIAL ? value * 1.609344 : value;
        long r = Math.round(km);
        return r < 0 ? 0 : (int)Math.min(r, 65535L);
    }

    static int tempToDc(float value, int unit) {
        double c = unit == UNIT_IMPERIAL ? (value - 32.0) * 5.0 / 9.0 : value;
        return (int)Math.round(c * 10.0);
    }

    /* ---------------- DSI bindings ---------------- */

    private static final class Binding {
        private final String label;
        private final Class service;
        private final Class listenerType;
        private final DSIListener listener;
        private final int[] attrs;
        private FrameworkRef.ServiceHandle handle;
        private ServiceRegistration registration;
        private boolean missLogged;

        Binding(String label, Class service, Class listenerType, DSIListener listener, int[] attrs) {
            this.label = label; this.service = service; this.listenerType = listenerType;
            this.listener = listener; this.attrs = attrs;
        }

        boolean isBound() { return handle != null; }

        boolean bind(FrameworkRef fw) {
            if (handle != null) return true;
            FrameworkRef.ServiceHandle h = fw.getServiceHandle(service);
            if (h == null || !service.isInstance(h.service())) {
                if (h != null) h.release();
                if (!missLogged) { missLogged = true; Log.i(TAG, label + " not available"); }
                return false;
            }
            ServiceRegistration reg = null;
            try {
                reg = fw.serviceManager().registerDSIListener(DSI_INSTANCE, listenerType.getName(), listener);
                invokeNotification(h.service(), true);
                handle = h;
                registration = reg;
                return true;
            } catch (Throwable t) {
                if (reg != null) { try { reg.unregister(); } catch (Throwable ignored) { } }
                h.release();
                Log.w(TAG, label + " bind failed: " + t);
                return false;
            }
        }

        void release() {
            FrameworkRef.ServiceHandle h = handle;
            ServiceRegistration reg = registration;
            handle = null;
            registration = null;
            if (h != null) {
                try { invokeNotification(h.service(), false); }
                catch (Throwable t) { Log.w(TAG, label + " clear notification failed: " + t); }
            }
            if (reg != null) { try { reg.unregister(); } catch (Throwable ignored) { } }
            if (h != null) h.release();
        }

        private void invokeNotification(Object s, boolean on) {
            org.dsi.ifc.base.DSIBase dsi = (org.dsi.ifc.base.DSIBase)s;
            if (on) dsi.setNotification(attrs, listener);
            else dsi.clearNotification(attrs, listener);
        }
    }

    /* ---------------- listeners ---------------- */

    private final class KombiListener extends KombiListenerBase {
        public void updateBCTotalCurrentRange(CarBCDistance d, int valid) {
            synchronized (state) {
                rangeKmTotal = valid == ATTR_VALID && d != null && d.distanceValueState == BC_VALID
                    ? distanceToKm(d.distanceValue, d.distanceUnit) : -1;
            }
        }

        public void updateBCCurrentRange1(CarBCCurrentRange r, int valid) {
            synchronized (state) {
                rangeKm1 = valid == ATTR_VALID && r != null && r.state == BC_VALID
                    ? distanceToKm(r.currentRangeValue, r.currentRangeUnit) : -1;
            }
        }

        /* Stock AbstractBCComponent trusts the DSI valid flag alone here: the kombi leaves
         * CarBCTemperature.state unset on this car (2026-10-10 log: no temperature with the
         * state check). */
        public void updateBCOutsideTemperature(CarBCTemperature t, int valid) {
            synchronized (state) {
                tempDc = valid == ATTR_VALID && t != null
                    ? tempToDc(t.temperatureValue, t.temperatureUnit) : Integer.MIN_VALUE;
            }
            if (!tempLogged) {
                tempLogged = true;
                Log.i(TAG, "outside temperature from DSI: valid=" + valid + " " + t);
            }
        }
    }

    private final class GeneralListener extends GeneralVehicleStatesListenerBase {
        public void updateReverseGear(boolean on, int valid) {
            synchronized (state) { reverse = valid == ATTR_VALID ? (on ? 1 : 0) : -1; }
        }

        public void updateVehicleStandstill(boolean on, int valid) {
            synchronized (state) { standstill = valid == ATTR_VALID && on; }
        }

        public void updateTankInfo(TankInfo t, int valid) {
            synchronized (state) { fuelWarning = valid == ATTR_VALID && t != null ? (t.fuelWarning ? 1 : 0) : -1; }
        }
    }

    private final class DynamicListener implements DSICarVehicleStatesListener {
        public void updateDynamicVehicleInfoHighFrequent(DynamicVehicleInfoHighFrequent d, int valid) {
            if (valid != ATTR_VALID || d == null) return;
            CarBCSpeed s = d.realVehicleSpeed != null && d.realVehicleSpeed.speedValueState == BC_VALID
                ? d.realVehicleSpeed : d.vehicleSpeed;
            if (s == null || s.speedValueState != BC_VALID) return;
            synchronized (state) {
                speedCms = speedToCms(s.speedValue, s.speedUnit);
                speedAt = System.currentTimeMillis();
            }
        }

        public void asyncException(int errorCode, String errorString, int requestType) { }
        public void updateDynamicVehicleInfoHighFrequentViewOptions(DynamicVehicleInfoHighFrequentViewOptions o, int v) { }
        public void updateDynamicVehicleInfoMidFrequentViewOptions(DynamicVehicleInfoMidFrequentViewOptions o, int v) { }
        public void updateDynamicVehicleInfoMidFrequent(DynamicVehicleInfoMidFrequent d, int v) { }
        public void updateDynamicVehicleInfoSCR(DynamicVehicleInfoSCR d, int v) { }
        public void updateDrvSchoolSystem(boolean on, int v) { }
        public void updateKeyData(KeyData d, int v) { }
        public void updateKeyViewOption(CarViewOption o, int v) { }
        public void updateOilLevelData(OilLevelData d, int v) { }
        public void updateOilLevelViewOption(CarViewOption o, int v) { }
        public void updateVINData(String vin, int v) { }
        public void updateSemiStaticVehicleData(SemiStaticVehicleData d, int v) { }
        public void updateSemiStaticVehicleDataViewOptions(SemiStaticDataViewOptions o, int v) { }
        public void updateVINViewOption(CarViewOption o, int v) { }
        public void updateVehicleInfoViewOptions(VehicleInfoViewOptions o, int v) { }
    }
}
