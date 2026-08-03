package de.audi.tghu.fwhmi;

import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.HMIService;
import de.audi.atip.hmi.event.ScreenDebugInfoEvent;
import de.audi.atip.hmi.view.IDisplayListener;
import de.esolutions.fw.util.commons.Buffer;
import de.esolutions.fw.util.commons.error.DumpInfoProvider;
import java.io.PrintStream;
import org.dsi.ifc.displaymanagement.DisplayContext;
import org.dsi.ifc.global.ResourceLocator;
import com.luka.carplay.probe.ClusterProbe;

public class DisplayManagerMIB2High extends DisplayManager implements IDisplayListener, IDisplayManagerKombiControl {
    private static final boolean SHOW_DM_INFO = Boolean.getBoolean("showDisplayManagerInfo");
    int[] visibleKDKs = new int[8];

    public DisplayManagerMIB2High(IFrameworkAccess iframeworkaccess) {
        super(iframeworkaccess);
        iframeworkaccess.getErrorMgr()
            .registerDumpInfoProvider(new DisplayManagerMIB2High.DisplayManagerProvider(this));

        for (int i = 0; i < this.visibleKDKs.length; i++) {
            this.visibleKDKs[i] = -1;
        }
    }

    protected int getMappedInternalContext(int i) {
        return i;
    }

    protected int getMappedExternalContext(int i) {
        return i;
    }

    protected void defineContexts() {
        if (ClusterProbe.on()) ClusterProbe.log("defineContexts: kombiType=" + this.framework.getKombiType());
        if (this.framework.getKombiType() == 4) {
            this.dc = new DisplayContext[158];
        } else {
            this.dc = new DisplayContext[82];   /* CarPlay cluster: ctx 80 (map+backing+maneuver) + ctx 81 (map only) */
        }

        this.dc[0] = new DisplayContext(0, new int[]{16});
        this.dc[1] = new DisplayContext(1, new int[]{16, 26});
        this.dc[2] = new DisplayContext(2, new int[]{16, 25});
        this.dc[7] = new DisplayContext(7, new int[]{16, 34});
        this.dc[3] = new DisplayContext(3, new int[]{16, 17});
        this.dc[4] = new DisplayContext(4, new int[]{16, 18});
        this.dc[5] = new DisplayContext(5, new int[]{16, 0});
        this.dc[6] = new DisplayContext(6, new int[]{16, 27});
        this.dc[8] = new DisplayContext(8, new int[]{16, 37});
        this.dc[9] = new DisplayContext(9, new int[]{16, 35});
        this.dc[33] = new DisplayContext(33, new int[]{16, 38});
        this.dc[10] = new DisplayContext(10, new int[]{16, 19});
        this.dc[11] = new DisplayContext(11, new int[]{16, 20, 21, 19});
        this.dc[12] = new DisplayContext(12, new int[]{16, 20, 22, 19});
        this.dc[13] = new DisplayContext(13, new int[]{16, 20, 23, 19});
        this.dc[14] = new DisplayContext(14, new int[]{16, 20, 19});
        this.dc[15] = new DisplayContext(15, new int[]{16, 24, 19});
        this.dc[16] = new DisplayContext(16, new int[]{16, 39});
        this.dc[17] = new DisplayContext(17, new int[]{16, 20, 21, 39});
        this.dc[18] = new DisplayContext(18, new int[]{16, 20, 22, 39});
        this.dc[19] = new DisplayContext(19, new int[]{16, 20, 23, 39});
        this.dc[20] = new DisplayContext(20, new int[]{16, 20, 39});
        this.dc[21] = new DisplayContext(21, new int[]{16, 24, 39});
        this.dc[22] = new DisplayContext(22, new int[]{16, 23, 20, 19});
        this.dc[23] = new DisplayContext(23, new int[]{16, 31});
        this.dc[24] = new DisplayContext(24, new int[]{16, 41});
        this.dc[25] = new DisplayContext(25, new int[]{16, 43});
        this.dc[26] = new DisplayContext(26, new int[]{16, 44});
        this.dc[27] = new DisplayContext(27, new int[]{16, 19, 38});
        this.dc[28] = new DisplayContext(28, new int[]{16, 39, 38});
        this.dc[29] = new DisplayContext(29, new int[]{16, 50, 19});
        this.dc[30] = new DisplayContext(30, new int[]{16, 50, 39});
        this.dc[31] = new DisplayContext(31, new int[]{16, 51});
        this.dc[32] = new DisplayContext(32, new int[]{16, 21});
        this.dc[35] = new DisplayContext(35, new int[]{36});
        this.dc[36] = new DisplayContext(36, new int[]{36, 26});
        this.dc[37] = new DisplayContext(37, new int[]{36, 25});
        this.dc[42] = new DisplayContext(42, new int[]{36, 34});
        this.dc[38] = new DisplayContext(38, new int[]{36, 17});
        this.dc[39] = new DisplayContext(39, new int[]{36, 18});
        this.dc[40] = new DisplayContext(40, new int[]{36, 0});
        this.dc[41] = new DisplayContext(41, new int[]{36, 27});
        this.dc[43] = new DisplayContext(43, new int[]{36, 37});
        this.dc[44] = new DisplayContext(44, new int[]{36, 35});
        this.dc[45] = new DisplayContext(45, new int[]{36, 19});
        this.dc[46] = new DisplayContext(46, new int[]{36, 20, 21, 19});
        this.dc[47] = new DisplayContext(47, new int[]{36, 20, 22, 19});
        this.dc[48] = new DisplayContext(48, new int[]{36, 20, 23, 19});
        this.dc[49] = new DisplayContext(49, new int[]{36, 20, 19});
        this.dc[50] = new DisplayContext(50, new int[]{36, 24, 19});
        this.dc[51] = new DisplayContext(51, new int[]{36, 39});
        this.dc[52] = new DisplayContext(52, new int[]{36, 20, 21, 39});
        this.dc[53] = new DisplayContext(53, new int[]{36, 20, 22, 39});
        this.dc[54] = new DisplayContext(54, new int[]{36, 20, 23, 39});
        this.dc[55] = new DisplayContext(55, new int[]{36, 20, 39});
        this.dc[56] = new DisplayContext(56, new int[]{36, 24, 39});
        this.dc[57] = new DisplayContext(57, new int[]{36, 31});
        this.dc[58] = new DisplayContext(58, new int[]{36, 43});
        this.dc[59] = new DisplayContext(59, new int[]{36, 44});
        this.dc[60] = new DisplayContext(60, new int[]{36, 19, 38});
        this.dc[61] = new DisplayContext(61, new int[]{36, 39, 38});
        this.dc[62] = new DisplayContext(62, new int[]{36, 50, 19});
        this.dc[63] = new DisplayContext(63, new int[]{36, 50, 39});
        this.dc[64] = new DisplayContext(64, new int[]{36, 51});
        this.dc[66] = new DisplayContext(66, new int[]{36, 21});
        this.dc[67] = new DisplayContext(67, new int[]{26});
        this.dc[68] = new DisplayContext(68, new int[]{25});
        this.dc[69] = new DisplayContext(69, new int[]{34});
        this.dc[71] = new DisplayContext(71, new int[]{27});
        this.dc[72] = new DisplayContext(72, new int[]{33});
        this.dc[76] = new DisplayContext(76, new int[]{58});
        this.dc[77] = new DisplayContext(
            77,
            new int[]{
                20,
                IDisplayManagerKombiControl.DISPLAYABLE_KDK_BACKGROUND_LARGE_STAGE,
                IDisplayManagerKombiControl.DISPLAYABLE_KDK_BACKGROUND_SMALL_STAGE,
                58
            }
        );
        this.dc[73] = new DisplayContext(
            73,
            new int[]{
                20,
                IDisplayManagerKombiControl.DISPLAYABLE_KDK_BACKGROUND_LARGE_STAGE,
                IDisplayManagerKombiControl.DISPLAYABLE_KDK_BACKGROUND_SMALL_STAGE
            }
        );
        this.dc[74] = new DisplayContext(
            74,
            new int[]{
                20,
                IDisplayManagerKombiControl.DISPLAYABLE_KDK_BACKGROUND_LARGE_STAGE,
                IDisplayManagerKombiControl.DISPLAYABLE_KDK_BACKGROUND_SMALL_STAGE,
                33
            }
        );
        this.dc[75] = new DisplayContext(75, new int[0]);
        this.dc[78] = new DisplayContext(78, new int[]{16, 59});

        /* --- CarPlay cluster — TWO custom contexts, A5-class only (kombiType != 4) ---
         *   z-order = array order: index 0 = TOP/front, last = BOTTOM/back.
         *
         *   dc[80] = {98, 101, 102, 99}  NAV active: maneuver(98) over the two KDK 987 backings
         *                                (101 = 328x180 for sport/full, 102 = 210x153 for popup),
         *                                over base(99)
         *   dc[81] = {99}                NO nav: base only
         *
         *   98  = maneuver graphics overlay (maneuver_render; EGL/GPU; transparent when idle)
         *   101 = stock KDK backing, 328x180 (Image) — used for the sport/full-buffer maneuver
         *   102 = stock KDK backing, 210x153 (Image) — used for the popup (cropped) maneuver
         *   99  = base layer (altscreen_render; empty when altScreen off)
         *
         *   NOTE (verified on-unit via dmdt gd): displayable 103 is NOT creatable (unregistered
         *   DSI id), but 101/102 ARE already created Image displayables carrying the 987 panel —
         *   so we use those.  Per-mode geometry (pos/crop/opacity of 98/101/102) is hardcoded in
         *   com.luka.carplay.cluster.ClusterLayerController.apply() (called from CombiMapController.handleKDK).
         *
         * Switched by CombiMapController on maneuver appear/disappear (80 <-> 81). */
        if (this.framework.getKombiType() != 4) {
            this.dc[80] = new DisplayContext(80, new int[]{98, 101, 102, 99});
            this.dc[81] = new DisplayContext(81, new int[]{99});
        }
        if (this.framework.getKombiType() == 4) {
            this.defineContextsForG24();
        }
    }

    private void defineContextsForG24() {
        for (int i = 0; i < 79; i++) {
            if (this.dc[i] != null) {
                byte b0 = 20;
                int[] aint = this.createG24ContextWithKDK(i, b0);
                this.dc[i + 79] = new DisplayContext(i + 79, aint);
            }
        }
    }

    private int[] createG24ContextWithKDK(int i, int j) {
        int k = -1;
        int[] aint = this.dc[i].getDisplayableList();

        for (int i1 = 0; i1 < aint.length; i1++) {
            int l = aint[i1];
            if (l == j) {
                k = i1;
                break;
            }
        }

        int[] aint1;
        if (k == -1) {
            aint1 = new int[aint.length + 1];
        } else {
            aint1 = new int[aint.length];
        }

        aint1[0] = j;
        int j1 = 1;

        for (int k1 = 0; k1 < aint.length; k1++) {
            if (aint[k1] != j) {
                aint1[j1] = aint[k1];
                j1++;
            }
        }

        return aint1;
    }

    protected void configureDM() {
        /* NOTE: our CarPlay cluster backing uses the stock Image displayables 101/102 — created
         * right here (below) because our A5 FPK unit reports sysConst(541)==2 (proven: 101/102
         * show in dmdt gd).  A fresh id 103 is NOT creatable (unregistered DSI id), so no
         * createImageDisplayable here for it. */
        if (this.framework.getSysConst(541) == 2) {
            this.createImageDisplayable(
                new ResourceLocator(
                    IDisplayManagerKombiControl.DISPLAYABLE_KDK_BACKGROUND_LARGE_STAGE,
                    "/mnt/app/eso/hmi/lsd/images/HMISystemEvoHigh/987.png"
                ),
                IDisplayManagerKombiControl.DISPLAYABLE_KDK_BACKGROUND_LARGE_STAGE
            );
            this.createImageDisplayable(
                new ResourceLocator(
                    IDisplayManagerKombiControl.DISPLAYABLE_KDK_BACKGROUND_SMALL_STAGE,
                    "/mnt/app/eso/hmi/lsd/images/HMISystemEvoHigh/987.png"
                ),
                IDisplayManagerKombiControl.DISPLAYABLE_KDK_BACKGROUND_SMALL_STAGE
            );
        }
    }

    public void setupKDKBackground(int i) {
    }

    public void activeContext(int i, int j) {
    }

    public void setKDKVisible(int i, int j) {
        this.log.log(1000000, "DisplayManager#setKDKVisible visibleKDK %1 terminal %2 ", i, j);
        if (this.visibleKDKs[j] != i) {
            this.visibleKDKs[j] = i;
            int k = this.getCurrentContextID(j);
            if (i == -1) {
                this.removeKDKFromContext(j, k);
            } else {
                this.switchContext(k, j, null);
            }
        }
    }

    private void removeKDKFromContext(int i, int j) {
        if (i == 0) {
            if (j >= 79) {
                this.switchContext(j % 79, 0, null);
            }
        } else if (i == 1 && this.framework.getSysConst(541) == 2) {
            if (j == 74) {
                this.switchContext(72, i, null);
            } else if (j == 77) {
                this.switchContext(76, i, null);
            } else if (j == 73) {
                this.switchContext(75, i, null);
            } else {
                this.switchContext(j, i, null);
            }
        }
    }

    public boolean isKDKVisible(int i) {
        return this.visibleKDKs[i] != -1;
    }

    public int getVisibleKDK(int i) {
        return this.visibleKDKs[i];
    }

    public synchronized void switchContext(int i, int j, IDisplayListener idisplaylistener) {
        int probeReq = i;
        this.log.log(1000000, "DisplayManager#switchContext visibleKDK %1 terminal %2 ", this.visibleKDKs[j], j);
        /* altScreen context (AltScreenModule CTX_CLUSTER = 80) is self-composed — it already
         * carries our planes (98/99/102/101).  It must NOT go through the stock KDK-20 remap:
         * addKDKToContext has no case for it and falls to default -> ctx 73, hijacking our
         * altScreen switch to a stock context (audit #8).  Every stock context is < 80, so
         * this guard is a no-op for all stock/RGI paths. */
        if (this.visibleKDKs[j] == 20 && i < 80) {
            i = this.addKDKToContext(i);
        }

        if (ClusterProbe.on()) ClusterProbe.onSwitchContext(probeReq, i, j, this.visibleKDKs[j], this.getDisplayables(i));
        super.switchContext(i, j, idisplaylistener);
        if (SHOW_DM_INFO && this.getCurrentContextID(j) > -1) {
            this.postStatisticInfoContextGeneral(i, j);
            this.postStatisticInfoContextOpacity(i, j);
        }
    }

    private void postStatisticInfoContextGeneral(int i, int j) {
        HMIService hmiservice = this.framework.getHMIService();
        String[] astring = this.createContextInfo(i);
        ScreenDebugInfoEvent screendebuginfoevent = new ScreenDebugInfoEvent(hmiservice.getRootWindow(0), astring, 19);
        hmiservice.getEventDispatcher().postEvent(screendebuginfoevent);
    }

    private String[] createContextInfo(int i) {
        int[] aint = this.getDisplayables(i);
        String[] astring = new String[aint.length + 1];
        astring[0] = new Buffer("DM: context = ").append(i).toString();

        for (int j = 0; j < aint.length; j++) {
            astring[j + 1] = new Buffer("displayable: ").append(j).append(" = ").append(aint[j]).toString();
        }

        return astring;
    }

    private void postStatisticInfoContextOpacity(int i, int j) {
        HMIService hmiservice = this.framework.getHMIService();
        String[] astring = this.createOpacityInfo(i, j);
        ScreenDebugInfoEvent screendebuginfoevent = new ScreenDebugInfoEvent(hmiservice.getRootWindow(0), astring, 20);
        hmiservice.getEventDispatcher().postEvent(screendebuginfoevent);
    }

    private String[] createOpacityInfo(int i, int j) {
        int[] aint = this.getDisplayables(i);
        String[] astring = new String[aint.length + 1];
        astring[0] = "DM: displayable opacity";

        for (int k = 0; k < aint.length; k++) {
            astring[k + 1] = new Buffer().append(aint[k]).append(" = ").append(this.getOpacity(aint[k], j)).toString();
        }

        return astring;
    }

    private int addKDKToContext(int i) {
        if (this.framework.getKombiType() == 4) {
            i += 79;
        } else if (this.framework.getSysConst(541) == 2) {
            switch (i) {
                case 72:
                    i = 74;
                    break;
                case 73:
                default:
                    i = 73;
                    break;
                case 74:
                    i = 74;
                    break;
                case 75:
                    i = 73;
                    break;
                case 76:
                    i = 77;
                    break;
                case 77:
                    i = 77;
            }

            this.log.log(1000000, "DisplayManager#addKDKToContext new context %1 ", i);
        }

        return i;
    }

    public void setKDKOpacity(int i, int j) {
        if (this.visibleKDKs[i] != -1) {
            super.setOpacity(this.visibleKDKs[i], i, j);
        }
    }

    public void setOpacity(int i, int j, int k) {
        super.setOpacity(i, j, k);
        if (SHOW_DM_INFO && this.getCurrentContextID(j) > -1) {
            this.postStatisticInfoContextOpacity(this.getCurrentContextID(j), j);
        }
    }

    private class DisplayManagerProvider implements DumpInfoProvider {
        private final DisplayManagerMIB2High this$0;

        private DisplayManagerProvider(DisplayManagerMIB2High displaymanagermib2high) {
            this.this$0 = displaymanagermib2high;
        }

        public String getName() {
            return "DisplayManager-Info";
        }

        public void dump(PrintStream printstream, String s) {
            this.dumpTerminalInfo(printstream, 0);
            this.dumpTerminalInfo(printstream, 1);
        }

        private void dumpTerminalInfo(PrintStream printstream, int i) {
            printstream.println("DM info for terminal: " + i);
            if (this.this$0.getCurrentContextID(i) != -1) {
                String[] astring = this.this$0.createContextInfo(this.this$0.getCurrentContextID(i));
                String[] astring1 = this.this$0.createOpacityInfo(this.this$0.getCurrentContextID(i), i);

                for (int j = 0; j < astring.length; j++) {
                    printstream.println(astring[j]);
                }

                for (int k = 0; k < astring1.length; k++) {
                    printstream.println(astring1[k]);
                }
            } else {
                printstream.println("no context activated ");
            }
        }
    }
}
