package com.luka.carplay.startup;

/** HMI identity ownership; model 4239 also gates TerminalMode/Media screens. */
public final class MenuIdentityState {
    private int generation, stock, stockType;
    private boolean managed, connected, shown;

    public void begin(int id) {
        // Retain an already published stock STARTED on ACTIVE recovery. Delaying
        // real readiness for our optional renderer can block the stock screen.
        if (managed || stockType != 1) stock = 0;
        generation = id; managed = connected = true; shown = false;
        stockType = 1;
    }
    public void shown(int id) { if (connected && generation == id) shown = true; }
    public void fallback(int id) { if (connected && generation == id) shown = false; }
    public void end(int id) {
        if (!managed || generation != id) return;
        connected = shown = false;
        if (stockType == 1) stock = 0;
    }
    public void stock(int value, int type) {
        // A delayed CarPlay exit must not erase a newly active Android/CarLife item.
        if (type == 1 && stockType != 1 && stock != 0) return;
        stockType = type;
        stock = value;
    }
    public int value() {
        if (!managed || stockType != 1) return stock;
        if (!connected) return 0; // Late stock STARTED cannot resurrect a disconnected attempt.
        if (shown) return 1;
        return stock;
    }
    public int selection(int selected) {
        if (selected < 16 || selected > 19) return selected;
        switch (value()) { case 1: return 16; case 2: return 17; case 3: return 19; default: return 18; }
    }
}
