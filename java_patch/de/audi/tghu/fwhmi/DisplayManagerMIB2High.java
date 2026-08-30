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

public class DisplayManagerMIB2High extends DisplayManager implements IDisplayListener, IDisplayManagerKombiControl {
    private static final boolean SHOW_DM_INFO = Boolean.getBoolean("showDisplayManagerInfo");
    /** Per-terminal id of the currently-visible KDK maneuver (DISPLAYABLE_KDK), or -1 if none. */
    int[] visibleKDKs = new int[8];

    /* ---- terminals ---- */
    private static final int MAIN_TERMINAL = 0;
    private static final int CLUSTER_TERMINAL = 1;

    /* ---- kombi variant ---- */
    private static final int KOMBI_TYPE_G24 = 4;                 // getKombiType(): single big display (G24)
    private static final int SYSCONST_KOMBI_VARIANT = 541;
    private static final int KOMBI_KDK_VIA_DISPLAYABLES = 2;     // A5-class: KDK composed from displayables

    /* ---- cluster display contexts ---- */
    private static final int CTX_MAP         = 72;   // map only
    private static final int CTX_KDK_NO_MAP  = 73;   // KDK + backings, no map
    private static final int CTX_MAP_KDK     = 74;   // map + KDK + backings
    private static final int CTX_BLANK       = 75;   // empty
    private static final int CTX_MAP_ALT     = 76;   // alt map only
    private static final int CTX_MAP_ALT_KDK = 77;   // alt map + KDK + backings
    private static final int CTX_CARPLAY_NAV = 80;   // CarPlay: maneuver + backing + stock native map
    private static final int FIRST_CARPLAY_CONTEXT = 80;   // every stock context id is < this

    /* ---- context-table sizing / G24 KDK variants ---- */
    private static final int DC_SIZE_A5  = 82;    // stock 0..78 + CarPlay 80
    private static final int DC_SIZE_G24 = 158;   // stock + the +79 KDK-hoisted variants
    private static final int G24_KDK_CTX_OFFSET = 79;
    private int lastBlockedCarPlayContext = -1;

    public DisplayManagerMIB2High(IFrameworkAccess iframeworkaccess) {
        super(iframeworkaccess);
        iframeworkaccess.getErrorMgr().registerDumpInfoProvider(new DisplayManagerProvider());

        for (int t = 0; t < this.visibleKDKs.length; t++) {
            this.visibleKDKs[t] = DISPLAYABLE_NONE;
        }
    }

    protected int getMappedInternalContext(int i) {
        return i;
    }

    protected int getMappedExternalContext(int i) {
        return i;
    }

    protected void defineContexts() {
        if (this.framework.getKombiType() == KOMBI_TYPE_G24) {
            this.dc = new DisplayContext[DC_SIZE_G24];
        } else {
            this.dc = new DisplayContext[DC_SIZE_A5];   // stock contexts + CarPlay ctx 80
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

        /* --- CarPlay cluster: one custom context, A5-class only (not G24) ---
         *   z-order = array order (index 0 = front):
         *     dc[80] = {98, 101, 102, 33}  nav active: maneuver(98) over the two KDK 987 backings
         *                                  (101 = 328x180 sport, 102 = 210x153 popup) over the stock
         *                                  native map (33)
         *   98 = maneuver overlay (maneuver_render, transparent when idle); there is NO CarPlay video
         *   plane — the maneuver overlay composites over the head unit's own native map (33), the same
         *   displayable the stock cluster context 74 already carries.
         *   Displayable 103 is NOT creatable (unregistered DSI id); 101/102 already exist as the 987
         *   Image backings, so we reuse those.  Plane geometry lives in ClusterLayerController; the
         *   74<->80 switch is driven by ScreenModule (no-nav state is plain stock ctx 74). */
        if (this.framework.getKombiType() != KOMBI_TYPE_G24) {
            this.dc[CTX_CARPLAY_NAV] = new DisplayContext(CTX_CARPLAY_NAV, new int[]{98, 101, 102, 33});
        } else {
            this.defineContextsForG24();
        }
    }

    /** G24 has no separate KDK layer, so every stock context gets a "+79" twin with the KDK
     *  maneuver (displayable 20) hoisted to the front. */
    private void defineContextsForG24() {
        for (int ctx = 0; ctx < G24_KDK_CTX_OFFSET; ctx++) {
            if (this.dc[ctx] != null) {
                int[] withKdk = this.createG24ContextWithKDK(ctx, DISPLAYABLE_KDK);
                this.dc[ctx + G24_KDK_CTX_OFFSET] = new DisplayContext(ctx + G24_KDK_CTX_OFFSET, withKdk);
            }
        }
    }

    /** Returns the displayable list of {@code ctx} with {@code kdk} moved to the front (index 0),
     *  growing the array by one only if the context did not already contain it. */
    private int[] createG24ContextWithKDK(int ctx, int kdk) {
        int[] base = this.dc[ctx].getDisplayableList();

        boolean alreadyPresent = false;
        for (int idx = 0; idx < base.length; idx++) {
            if (base[idx] == kdk) {
                alreadyPresent = true;
                break;
            }
        }

        int[] result = new int[alreadyPresent ? base.length : base.length + 1];
        result[0] = kdk;
        int writeIdx = 1;
        for (int idx = 0; idx < base.length; idx++) {
            if (base[idx] != kdk) {
                result[writeIdx++] = base[idx];
            }
        }
        return result;
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

    public void setKDKVisible(int kdk, int terminal) {
        this.log.log(1000000, "DisplayManager#setKDKVisible visibleKDK %1 terminal %2 ", kdk, terminal);
        if (this.visibleKDKs[terminal] != kdk) {
            this.visibleKDKs[terminal] = kdk;
            int currentCtx = this.getCurrentContextID(terminal);
            if (kdk == DISPLAYABLE_NONE) {
                this.removeKDKFromContext(terminal, currentCtx);
            } else {
                this.switchContext(currentCtx, terminal, null);
            }
        }
    }

    /** KDK just became invisible: drop back to the no-KDK sibling of the current context. */
    private void removeKDKFromContext(int terminal, int ctx) {
        if (terminal == MAIN_TERMINAL) {
            if (ctx >= G24_KDK_CTX_OFFSET) {
                this.switchContext(ctx % G24_KDK_CTX_OFFSET, MAIN_TERMINAL, null);   // drop the +79 KDK twin
            }
        } else if (terminal == CLUSTER_TERMINAL
                && this.framework.getSysConst(SYSCONST_KOMBI_VARIANT) == KOMBI_KDK_VIA_DISPLAYABLES) {
            if (ctx == CTX_MAP_KDK) {
                this.switchContext(CTX_MAP, terminal, null);
            } else if (ctx == CTX_MAP_ALT_KDK) {
                this.switchContext(CTX_MAP_ALT, terminal, null);
            } else if (ctx == CTX_KDK_NO_MAP) {
                this.switchContext(CTX_BLANK, terminal, null);
            } else {
                this.switchContext(ctx, terminal, null);
            }
        }
    }

    public boolean isKDKVisible(int i) {
        return this.visibleKDKs[i] != -1;
    }

    public int getVisibleKDK(int i) {
        return this.visibleKDKs[i];
    }

    public synchronized void switchContext(int ctx, int terminal, IDisplayListener listener) {
        this.log.log(1000000, "DisplayManager#switchContext visibleKDK %1 terminal %2 ", this.visibleKDKs[terminal], terminal);
        /* Every stock VC screen controller eventually funnels through this method, not only
         * CombiMapController.  While CarPlay owns terminal 1, accept physical context writes only
         * from ScreenModule's one serialized worker.  The logical FwHMI main-context still
         * changes normally and RouteGuidance uses it to enter ctx80 while CarPlay owns the cluster. */
        if (terminal == CLUSTER_TERMINAL
                && com.luka.carplay.core.ScreenModule.isConnected()
                && !com.luka.carplay.core.ScreenModule.isClusterContextWriterThread()) {
            if (lastBlockedCarPlayContext != ctx) {
                lastBlockedCarPlayContext = ctx;
                this.log.log(1000000,
                    "DisplayManager#switchContext blocked stock context %1 while CarPlay owns terminal %2 ",
                    ctx, terminal);
            }
            return;
        }
        lastBlockedCarPlayContext = -1;
        /* Stock KDK-20 remap: while a KDK maneuver is visible, a switch to a stock map context is
         * redirected to its KDK-carrying twin (addKDKToContext).  Our CarPlay contexts (>= 80) are
         * self-composed — they already carry planes 98/101/102/33 — and MUST skip the remap, else
         * addKDKToContext's default case would hijack the switch to ctx 73.  Every stock context id
         * is < 80, so this guard is a no-op for all stock/RGI paths. */
        if (this.visibleKDKs[terminal] == DISPLAYABLE_KDK && ctx < FIRST_CARPLAY_CONTEXT) {
            ctx = this.addKDKToContext(ctx);
        }

        super.switchContext(ctx, terminal, listener);
        if (SHOW_DM_INFO && this.getCurrentContextID(terminal) > -1) {
            this.postStatisticInfoContextGeneral(ctx, terminal);
            this.postStatisticInfoContextOpacity(ctx, terminal);
        }
    }

    private void postStatisticInfoContextGeneral(int ctx, int terminal) {
        HMIService hmiservice = this.framework.getHMIService();
        String[] lines = this.createContextInfo(ctx);
        ScreenDebugInfoEvent event = new ScreenDebugInfoEvent(hmiservice.getRootWindow(0), lines, 19);
        hmiservice.getEventDispatcher().postEvent(event);
    }

    private String[] createContextInfo(int ctx) {
        int[] displayables = this.getDisplayables(ctx);
        String[] lines = new String[displayables.length + 1];
        lines[0] = new Buffer("DM: context = ").append(ctx).toString();

        for (int idx = 0; idx < displayables.length; idx++) {
            lines[idx + 1] = new Buffer("displayable: ").append(idx).append(" = ").append(displayables[idx]).toString();
        }

        return lines;
    }

    private void postStatisticInfoContextOpacity(int ctx, int terminal) {
        HMIService hmiservice = this.framework.getHMIService();
        String[] lines = this.createOpacityInfo(ctx, terminal);
        ScreenDebugInfoEvent event = new ScreenDebugInfoEvent(hmiservice.getRootWindow(0), lines, 20);
        hmiservice.getEventDispatcher().postEvent(event);
    }

    private String[] createOpacityInfo(int ctx, int terminal) {
        int[] displayables = this.getDisplayables(ctx);
        String[] lines = new String[displayables.length + 1];
        lines[0] = "DM: displayable opacity";

        for (int idx = 0; idx < displayables.length; idx++) {
            lines[idx + 1] = new Buffer().append(displayables[idx]).append(" = ").append(this.getOpacity(displayables[idx], terminal)).toString();
        }

        return lines;
    }

    /** Map a stock cluster context to the twin that also carries the KDK maneuver. */
    private int addKDKToContext(int ctx) {
        if (this.framework.getKombiType() == KOMBI_TYPE_G24) {
            return ctx + G24_KDK_CTX_OFFSET;
        }
        if (this.framework.getSysConst(SYSCONST_KOMBI_VARIANT) == KOMBI_KDK_VIA_DISPLAYABLES) {
            switch (ctx) {
                case CTX_MAP:         ctx = CTX_MAP_KDK;     break;   // 72 -> 74
                case CTX_MAP_KDK:     ctx = CTX_MAP_KDK;     break;   // 74 -> 74
                case CTX_BLANK:       ctx = CTX_KDK_NO_MAP;  break;   // 75 -> 73
                case CTX_MAP_ALT:     ctx = CTX_MAP_ALT_KDK; break;   // 76 -> 77
                case CTX_MAP_ALT_KDK: ctx = CTX_MAP_ALT_KDK; break;   // 77 -> 77
                case CTX_KDK_NO_MAP:                                  // 73 -> 73 (shares default)
                default:              ctx = CTX_KDK_NO_MAP;  break;   // anything else -> 73
            }
            this.log.log(1000000, "DisplayManager#addKDKToContext new context %1 ", ctx);
        }
        return ctx;
    }

    public void setKDKOpacity(int terminal, int opacity) {
        if (this.visibleKDKs[terminal] != DISPLAYABLE_NONE) {
            super.setOpacity(this.visibleKDKs[terminal], terminal, opacity);
        }
    }

    public void setOpacity(int displayable, int terminal, int opacity) {
        super.setOpacity(displayable, terminal, opacity);
        if (SHOW_DM_INFO && this.getCurrentContextID(terminal) > -1) {
            this.postStatisticInfoContextOpacity(this.getCurrentContextID(terminal), terminal);
        }
    }

    private class DisplayManagerProvider implements DumpInfoProvider {
        public String getName() {
            return "DisplayManager-Info";
        }

        public void dump(PrintStream out, String s) {
            this.dumpTerminalInfo(out, MAIN_TERMINAL);
            this.dumpTerminalInfo(out, CLUSTER_TERMINAL);
        }

        private void dumpTerminalInfo(PrintStream out, int terminal) {
            out.println("DM info for terminal: " + terminal);
            int ctx = getCurrentContextID(terminal);
            if (ctx == -1) {
                out.println("no context activated ");
                return;
            }
            String[] ctxLines = createContextInfo(ctx);
            String[] opacityLines = createOpacityInfo(ctx, terminal);
            for (int i = 0; i < ctxLines.length; i++) {
                out.println(ctxLines[i]);
            }
            for (int i = 0; i < opacityLines.length; i++) {
                out.println(opacityLines[i]);
            }
        }
    }
}
