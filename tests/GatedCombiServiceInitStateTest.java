package com.luka.carplay.rgd;

import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/** Regression coverage for the cold-boot NAVSD INITIALIZING takeover. */
public final class GatedCombiServiceInitStateTest {
    private static final class Recorder implements InvocationHandler {
        int showCount;
        int hideCount;

        public Object invoke(Object proxy, Method method, Object[] args) {
            String name = method.getName();
            if ("showInitializingScreen".equals(name)) showCount++;
            else if ("hideInitializingScreen".equals(name)) hideCount++;
            return null;
        }

        CombiBAPServiceNavi service() {
            return (CombiBAPServiceNavi)Proxy.newProxyInstance(
                CombiBAPServiceNavi.class.getClassLoader(),
                new Class[]{CombiBAPServiceNavi.class},
                this
            );
        }
    }

    private static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static void testStockForwarding() {
        Recorder recorder = new Recorder();
        GatedCombiService gate = new GatedCombiService(recorder.service());
        gate.showInitializingScreen();
        gate.hideInitializingScreen();
        require(recorder.showCount == 1, "stock show must pass before takeover");
        require(recorder.hideCount == 1, "stock hide must pass before takeover");
    }

    private static void testDeferredInitializingRestored() {
        Recorder recorder = new Recorder();
        GatedCombiService gate = new GatedCombiService(recorder.service());

        gate.showInitializingScreen();
        gate.setAltScreenTakeover(true);
        require(recorder.showCount == 1, "takeover must not emit another show");
        require(recorder.hideCount == 1, "takeover must clear pre-existing INITIALIZING");

        gate.showInitializingScreen();
        require(recorder.showCount == 1, "stock INITIALIZING must be deferred during takeover");

        gate.setAltScreenTakeover(false);
        require(recorder.showCount == 2, "release must restore deferred INITIALIZING");
    }

    private static void testDeferredReadyRestored() {
        Recorder recorder = new Recorder();
        GatedCombiService gate = new GatedCombiService(recorder.service());

        gate.setAltScreenTakeover(true);
        require(recorder.hideCount == 1, "takeover must assert READY once");
        gate.showInitializingScreen();
        gate.hideInitializingScreen();
        require(recorder.showCount == 0, "crossing stock show must remain hidden");
        require(recorder.hideCount == 2, "stock READY must be reasserted during takeover");

        gate.setAltScreenTakeover(false);
        require(recorder.hideCount == 3, "release must restore latest deferred READY");
    }

    private static void testIdempotentTakeoverAndNoEdgeRelease() {
        Recorder recorder = new Recorder();
        GatedCombiService gate = new GatedCombiService(recorder.service());

        gate.setAltScreenTakeover(true);
        gate.setAltScreenTakeover(true);
        require(recorder.hideCount == 1, "duplicate takeover must not resend READY");
        gate.setAltScreenTakeover(false);
        require(recorder.hideCount == 1 && recorder.showCount == 0,
            "no-edge release is recomputed by ClusterService, not guessed by the wrapper");
    }

    public static void main(String[] args) {
        testStockForwarding();
        testDeferredInitializingRestored();
        testDeferredReadyRestored();
        testIdempotentTakeoverAndNoEdgeRelease();
        System.out.println("GatedCombiService init-state tests: PASS");
    }
}
