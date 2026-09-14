package com.luka.carplay.core;

import de.audi.app.terminalmode.IContext;
import de.audi.atip.base.IFrameworkAccess;

/* Only external services/modules are fake; tests compile the real CarPlayApp. */
abstract class LifecycleTestModule implements Module {
    static final Object gate = new Object();
    static final int[] starts = new int[3], stops = new int[3];
    static int blockIndex = -1, entered = -1;
    static boolean released;
    static volatile boolean badLockOrder;
    final int index;
    LifecycleTestModule(int i) { index = i; }
    public String name() { return "lifecycle-test-" + index; }
    private void checkLock() {
        if (Thread.holdsLock(CarPlayAppLifecycleTest.stateLock)) {
            badLockOrder = true;
            throw new AssertionError("external module called under state lock");
        }
    }
    public boolean start(FrameworkRef fw) {
        checkLock();
        synchronized (gate) {
            starts[index]++;
            if (blockIndex == index) {
                entered = index;
                gate.notifyAll();
                long deadline = System.currentTimeMillis() + 5000;
                while (!released) {
                    long remaining = deadline - System.currentTimeMillis();
                    if (remaining <= 0) throw new AssertionError("module release timeout");
                    try { gate.wait(remaining); }
                    catch (InterruptedException x) { throw new AssertionError(x); }
                }
                blockIndex = -1;
            }
        }
        return true;
    }
    public void stop() { checkLock(); synchronized (gate) { stops[index]++; } }
    static int count(int index) { synchronized (gate) { return starts[index]; } }
}
final class AltScreenModule extends LifecycleTestModule { AltScreenModule() { super(0); } }
final class RgdModule extends LifecycleTestModule { RgdModule() { super(1); } }
final class SteeringWheelInputModule extends LifecycleTestModule { SteeringWheelInputModule() { super(2); } }
final class AltscreenControlServer { static void start() { } }
final class FrameworkRef {
    static volatile boolean failNext;
    FrameworkRef(IContext context) {
        if (failNext) { failNext = false; throw new IllegalStateException("injected framework failure"); }
    }
    boolean isReady() { return true; }
    IFrameworkAccess framework() { return null; }
}
