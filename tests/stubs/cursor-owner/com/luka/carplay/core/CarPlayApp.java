package com.luka.carplay.core;
import de.audi.atip.base.IFrameworkAccess;
public final class CarPlayApp {
    public static boolean active = true;
    public static int generation = 10;
    public static boolean isActive() { return active; }
    public static boolean isCurrentSession(int id) { return active && generation == id; }
    public static IFrameworkAccess framework() { return null; }
}
