/*
 * AppState — patched (navignore_audi.jar fix, ported from the working old impl).
 *
 * getAppStateID() and getOwner() ALWAYS return 0, so the stock system never detects CarPlay
 * taking navigation ownership.  Without this the stock CarPlay-nav path auto-switches the VC
 * into its own compass/RGI view when CarPlay navigation starts, which FIGHTS our BAP-driven
 * route guidance (the cluster RG never gets driven by us → "RGI BAP не идёт").  Neutralising the
 * ownership signal keeps the VC controllable by our BAPBridge.  Only these two getters differ
 * from the stock class; all fields/ctors/toString are the stock MU1316 versions.
 */
package org.dsi.ifc.carplay;

public class AppState {
    public int appStateID;
    public int owner;
    public int speechMode;

    public AppState() {
        this.appStateID = 0;
        this.owner = 0;
        this.speechMode = 0;
    }

    public AppState(int i, int j, int k) {
        this.appStateID = i;
        this.owner = j;
        this.speechMode = k;
    }

    /** Always 0 — prevents the stock system from detecting CarPlay nav-ownership. */
    public int getAppStateID() {
        return 0;
    }

    /** Always 0 — prevents the stock system from detecting CarPlay nav-ownership. */
    public int getOwner() {
        return 0;
    }

    public int getSpeechMode() {
        return this.speechMode;
    }

    public String toString() {
        StringBuffer stringbuffer = new StringBuffer(150);
        stringbuffer.append("AppState");
        stringbuffer.append('(');
        stringbuffer.append("appStateID");
        stringbuffer.append('=');
        stringbuffer.append(this.appStateID);
        stringbuffer.append(',');
        stringbuffer.append("owner");
        stringbuffer.append('=');
        stringbuffer.append(this.owner);
        stringbuffer.append(',');
        stringbuffer.append("speechMode");
        stringbuffer.append('=');
        stringbuffer.append(this.speechMode);
        stringbuffer.append(')');
        return stringbuffer.toString();
    }
}
