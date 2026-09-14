package com.luka.carplay.bus;
public final class CarplayBus {
    public static final int FLAG_BINARY = 2;
    public static final int EVT_HU_VIDEO_STATUS = 0x31, CMD_HU_VIDEO_STATUS = 0x130;
    public interface Listener { void onFrame(int type, int flags, byte[] p, int len); }
    private static final CarplayBus instance = new CarplayBus();
    public static CarplayBus getInstance() { return instance; }
    public byte[] lastQuery;
    public void on(int type, Listener l) { }
    public boolean send(int type, int flags, byte[] p, int len) { lastQuery = p; return true; }
}
