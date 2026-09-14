package com.luka.carplay.bus;
public final class CarplayBus {
    private static final CarplayBus INSTANCE = new CarplayBus();
    public static CarplayBus getInstance() { return INSTANCE; }
    public void start() { }
}
