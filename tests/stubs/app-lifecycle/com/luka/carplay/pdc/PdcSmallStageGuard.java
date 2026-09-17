package com.luka.carplay.pdc;

/** UI boundary for the isolated module lifecycle tests. test_pdc.sh separately
 * runs real CarPlayApp teardown with the shipping guard and drawer service. */
public final class PdcSmallStageGuard {
    public static void carPlayDisconnected() { }
}
