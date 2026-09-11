package com.luka.carplay.core;

/* Host control-channel test only; never included in the shipping patch JAR. */
public final class AltScreenModule {
    static volatile boolean video;
    static final java.util.concurrent.CountDownLatch shown = new java.util.concurrent.CountDownLatch(1);
    public static void setVideoAvailable(boolean on) { video = on; if (on) shown.countDown(); }
}
