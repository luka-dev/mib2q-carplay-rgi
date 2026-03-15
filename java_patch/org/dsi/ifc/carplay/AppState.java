/*
 * AppState -- patched to prevent nav focus transfer to CarPlay.
 *
 * getAppStateID() and getOwner() always return 0, so the system
 * never detects CarPlay taking navigation ownership.
 * This keeps the VC in map view instead of switching to compass/RGI
 * when CarPlay navigation starts.
 *
 * From navignore_audi.jar (pre-existing fix).
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

    public AppState(int appStateID, int owner, int speechMode) {
        this.appStateID = appStateID;
        this.owner = owner;
        this.speechMode = speechMode;
    }

    /** Always returns 0 -- prevents nav focus transfer to CarPlay. */
    public int getAppStateID() {
        return 0;
    }

    /** Always returns 0 -- prevents nav ownership detection. */
    public int getOwner() {
        return 0;
    }

    public int getSpeechMode() {
        return this.speechMode;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer(150);
        sb.append("AppState");
        sb.append('(');
        sb.append("appStateID");
        sb.append('=');
        sb.append(this.appStateID);
        sb.append(',');
        sb.append("owner");
        sb.append('=');
        sb.append(this.owner);
        sb.append(',');
        sb.append("speechMode");
        sb.append('=');
        sb.append(this.speechMode);
        sb.append(')');
        return sb.toString();
    }
}
