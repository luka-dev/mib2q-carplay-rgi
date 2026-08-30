package de.esolutions.hmi.widgets.audi.evo.high.widgets;

import de.audi.atip.hmi.event.ModelUpdateEvent;
import de.audi.atip.hmi.intercommunication.NaviMoKoKDKConstants;
import de.audi.atip.hmi.model.HMIModel;
import de.audi.atip.hmi.modelaccess.ChoiceModelGUI;
import de.audi.atip.hmi.view.IDisplayManager;
import de.audi.atip.model.ICoreNaviModelBank;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;
import de.audi.tghu.hmi.evo.IHMIServiceEvo;
import de.esolutions.hmi.widgets.audi.base.HMITerminalImpl;
import de.esolutions.hmi.widgets.audi.base.InitializationContext;
import de.esolutions.hmi.widgets.audi.base.Layout;
import de.esolutions.hmi.widgets.audi.base.widgets.IRenderer;
import de.esolutions.hmi.widgets.audi.evo.DisplayControllerEvo;
import de.esolutions.hmi.widgets.audi.evo.IAnimationTypesEvo;
import de.esolutions.hmi.widgets.audi.evo.high.AnimationControllerMIB2High;

/**
 * Instrument-cluster (Kombi) map controller for MIB2-High, with the CarPlay maneuver overlay.
 *
 * <p>Drives two things on the cluster terminal (terminal 1) via the {@link IDisplayManagerKombiControl}:
 * <ul>
 *   <li>the stock <b>KDK</b> maneuver panel (displayable {@link #KDK_MANEUVER}=20) and its two
 *       background images ({@link #KDK_BG_SPORT}=101 / {@link #KDK_BG_POPUP}=102), positioned/cropped
 *       per the model-update hint bits, and</li>
 *   <li>the <b>CarPlay</b> maneuver overlay — delegated entirely to
 *       {@link com.luka.carplay.cluster.ClusterLayerController}; this class only forwards the
 *       popup/sport stage on each KDK update.</li>
 * </ul>
 *
 * <p>Stage terminology follows the firmware hint bit {@code BITFIELD_KDK_POSITION_IN_TUBE}: set = the
 * maneuver is drawn large inside the round gauge ("in tube" / sport), clear = the small popup.  NOTE
 * the firmware background-id constant names are the inverse of their pixel size — {@code SMALL_STAGE}
 * is 101 (the 328x180 sport panel) and {@code LARGE_STAGE} is 102 (the 210x153 popup); the aliases
 * below name them by what they actually are.
 */
public class CombiMapController extends DisplayControllerEvo implements NaviMoKoKDKConstants {

    /* Cluster displayables & opacity levels (values mirror IDisplayManagerKombiControl; note that
     * interface's KDK background names are inverted vs pixel size — its "SMALL_STAGE" is the larger
     * 328x180 sport panel). */
    private static final int NO_KDK       = -1;    // no maneuver shown
    private static final int KDK_MANEUVER = 20;    // stock KDK maneuver
    private static final int KDK_BG_SPORT = 101;   // SMALL_STAGE bg, 328x180 (sport / in tube)
    private static final int KDK_BG_POPUP = 102;   // LARGE_STAGE bg, 210x153 (popup)
    private static final int MAP_MAIN     = 33;    // cluster map (ctx 72/74)
    private static final int MAP_ALT      = 58;    // cluster map (ctx 76/77)
    private static final int OPACITY_HIDDEN_FULL  = 0;    // fully transparent
    private static final int OPACITY_VISIBLE_FULL = 100;  // fully opaque

    /* ---- Layout.getIntegerConstant() slot indices (pixel values live in the layout resource) ---- */
    private static final int LC_SPORT_X = 58, LC_SPORT_Y = 59;    // sport anchor (bg 101 + sport KDK dst)
    private static final int LC_POPUP_X = 60, LC_POPUP_Y = 61;    // popup anchor (bg 102 + popup KDK dst)
    private static final int LC_SMALL_STAGE_MAP_DX = 80, LC_SMALL_STAGE_MAP_DY = 81;      // small-stage map offset
    private static final int LC_POPUP_CROP_X = 118, LC_POPUP_CROP_Y = 119, LC_POPUP_CROP_W = 120, LC_POPUP_CROP_H = 121;
    private static final int LC_SPORT_CROP_X = 122, LC_SPORT_CROP_Y = 123, LC_SPORT_CROP_W = 124, LC_SPORT_CROP_H = 125;
    private static final int LC_MAP_X = IAnimationTypesEvo.TYPE_3D_DRIVESELECT_GRID_ANIMATION;  // reused as a layout index
    private static final int LC_MAP_Y = IAnimationTypesEvo.NUM_ANIMATION_TYPES;

    /* ---- cluster variant + contexts ---- */
    private static final int SYSCONST_KOMBI_VARIANT = 541;
    private static final int KOMBI_KDK_VIA_DISPLAYABLES = 2;   // A5-class: KDK is composed from displayables
    private static final int MAP_CONTEXT_MIN = 72, MAP_CONTEXT_MAX = 77;   // valid cluster map contexts
    private static final int CLUSTER_TERMINAL = 1;

    /* ---- frame rates ---- */
    private static final int FPS_FULL = 10, FPS_REDUCED = 1, FPS_OFF = 0;
    private static final int CONTEXT_BLANK = 75;   // empty context -> no update rate

    private int kombiTerminal = CLUSTER_TERMINAL;
    private int displayType;
    private int updateRate = FPS_FULL;
    private int visibleKdk = NO_KDK;   // KDK_MANEUVER while a maneuver is shown, else -1
    private int kdkPosX = 0;
    private int kdkPosY = 0;
    private int kdkOpacity = 0;

    public IRenderer getRenderer() {
        return null;
    }

    public void connected(InitializationContext initializationcontext) {
        super.connected(initializationcontext);
        if (this.kombiTerminal != CLUSTER_TERMINAL) {
            return;
        }
        IDisplayManager dm = displayManager(hmiService);
        Layout layout = layoutOf(this.terminal);
        if (dm != null && layout != null) {
            dm.setDisplayType(1, this.displayType);
            this.positionKdkBackgrounds(dm, layout);
            if (dm instanceof IDisplayManagerKombiControl) {
                /* Cache the skin-selected OEM KDK geometry before the first model delta. */
                com.luka.carplay.cluster.ClusterLayerController.updateLayout(
                    (IDisplayManagerKombiControl)dm, this.kombiTerminal, layout);
            }
            this.onViewSizeChanged(); /* seed current fullscreen/smallscreen state, not a hardcoded fullscreen */
        }
    }

    /* ============================================================
     * Model-update routing
     * ============================================================ */

    public void processModelUpdateEvent(ModelUpdateEvent modelupdateevent) {
        logKDK.log(1000000, "CombiMapController#processModelUpdateEvent %1", modelupdateevent);

        if (modelupdateevent.getModelId() == ICoreNaviModelBank.NAV_VIEW_SIZE_CHOICE) {
            this.onViewSizeChanged();
        } else if (this.kombiTerminal != 0) {
            /* cluster terminal: on A5 (KDK via displayables) recompute the KDK layout, then switch
             * context; other variants only need the context + frame-rate update. */
            if (this.framework.getSysConst(SYSCONST_KOMBI_VARIANT) == KOMBI_KDK_VIA_DISPLAYABLES) {
                this.handleKdk(modelupdateevent);
                this.switchToTargetContext();
            } else {
                this.switchToTargetContext();
                this.updateFrameRate(modelupdateevent);
            }
        } else {
            this.handleKdk(modelupdateevent);   // MAIN terminal (G24)
        }
    }

    private void onViewSizeChanged() {
        if (this.kombiTerminal == 0) {
            return;
        }
        IDisplayManager dm = displayManager(hmiService);
        Layout layout = layoutOf(this.terminal);
        if (dm == null || layout == null || hmiService == null) {
            logKDK.log(100000, "CombiMapController#processModelUpdateEvent environment not fully initialized");
            return;
        }
        HMIModel model = hmiService.getModel(ICoreNaviModelBank.NAV_VIEW_SIZE_CHOICE);
        boolean smallStage = (model instanceof ChoiceModelGUI) && ((ChoiceModelGUI) model).getValue() == 1;
        /* The Audi View button toggles the native cluster map full/small stage.  Record it so the
         * maneuver overlay follows the same stage; positionMap keeps the stock plane geometry in step. */
        com.luka.carplay.core.ScreenModule.setViewAreaMode(
            smallStage
                ? com.luka.carplay.core.ScreenModule.VIEWAREA_SMALLSCREEN
                : com.luka.carplay.core.ScreenModule.VIEWAREA_FULLSCREEN);
        this.positionMap(dm, layout, smallStage);
    }

    /* ============================================================
     * KDK maneuver panel
     * ============================================================ */

    private void handleKdk(ModelUpdateEvent modelupdateevent) {
        IDisplayManagerKombiControl dm = kombiDisplayManager();
        if (dm == null) {
            logKDK.log(100000, "CombiMapController#handleKDK failed to retrieve the display manager; ignoring the invocation");
            return;
        }

        int previousKdk = this.updateVisibleKdk(modelupdateevent);
        Layout layout = this.terminal.getLayout();
        boolean inTube = (modelupdateevent.getHints() & BITFIELD_KDK_POSITION_IN_TUBE) != 0;

        if (this.kombiTerminal != 0) {
            this.applyKdkDualTerminal(dm, this.visibleKdk, modelupdateevent, layout, inTube);
        } else {
            this.applyKdkSingleTerminal(this.visibleKdk, previousKdk, layout);
        }

        /* CarPlay overlay: 98 is ours; 101/102 are shared stock KDK backings. The controller
         * caches the stock values, overrides them only while CarPlay owns the cluster, and restores
         * them on release. popup == !inTube. */
        if (this.kombiTerminal != 0
                && this.framework.getSysConst(SYSCONST_KOMBI_VARIANT) == KOMBI_KDK_VIA_DISPLAYABLES) {
            com.luka.carplay.cluster.ClusterLayerController.apply(
                dm, this.kombiTerminal, layout, !inTube,
                this.visibleKdk != NO_KDK, this.kdkOpacity);
        }
    }

    /** Latch the KDK visibility from the hints; returns the previous value. */
    private int updateVisibleKdk(ModelUpdateEvent modelupdateevent) {
        int previous = this.visibleKdk;
        this.visibleKdk = (modelupdateevent.getHints() & BITFIELD_KDK_VISIBLE) != 0
            ? KDK_MANEUVER : NO_KDK;
        logKDK.log(10000000,
            "CombiMapController#updateKdkDisplayable received model update, previous visible kdk: %1 new visible kdk: %2",
            previous, this.visibleKdk);
        /* NOTE: CarPlay nav-active is NOT derived from this stock KDK-visible bit — that only says
         * "the stock cluster is drawing a KDK panel".  The authoritative flip lives in RouteGuidance
         * (rgActive == the BAP nav-active message) -> ScreenModule.setNavActive(). */
        return previous;
    }

    /** Cluster (dual-terminal) KDK layout: crop + position the maneuver and toggle the two backgrounds. */
    private void applyKdkDualTerminal(IDisplayManagerKombiControl dm, int kdk, ModelUpdateEvent modelupdateevent,
                                      Layout layout, boolean inTube) {
        int visibleBg = inTube ? KDK_BG_SPORT : KDK_BG_POPUP;
        int hiddenBg  = inTube ? KDK_BG_POPUP : KDK_BG_SPORT;
        int visibleBgOpacity = OPACITY_HIDDEN_FULL;

        if (kdk != NO_KDK) {
            this.kdkOpacity = (modelupdateevent.getHints() & BITFIELD_KDK_FADED_IN) != 0
                ? OPACITY_VISIBLE_FULL : OPACITY_HIDDEN_FULL;
            visibleBgOpacity = this.kdkOpacity;

            int cropSrcX, cropSrcY, cropW, cropH;
            if (inTube) {
                cropSrcX = layout.getIntegerConstant(LC_SPORT_CROP_X);
                cropSrcY = layout.getIntegerConstant(LC_SPORT_CROP_Y);
                cropW    = layout.getIntegerConstant(LC_SPORT_CROP_W);
                cropH    = layout.getIntegerConstant(LC_SPORT_CROP_H);
                this.kdkPosX = layout.getIntegerConstant(LC_SPORT_X);
                this.kdkPosY = layout.getIntegerConstant(LC_SPORT_Y);
            } else {
                cropSrcX = layout.getIntegerConstant(LC_POPUP_CROP_X);
                cropSrcY = layout.getIntegerConstant(LC_POPUP_CROP_Y);
                cropW    = layout.getIntegerConstant(LC_POPUP_CROP_W);
                cropH    = layout.getIntegerConstant(LC_POPUP_CROP_H);
                this.kdkPosX = layout.getIntegerConstant(LC_POPUP_X);
                this.kdkPosY = layout.getIntegerConstant(LC_POPUP_Y);
            }

            logKDK.log(10000000, "CombiMapController#handleKdkDualTerminal KDK cropping source: (%1, %2), size: %3 x %4",
                String.valueOf(cropSrcX), String.valueOf(cropSrcY), String.valueOf(cropW), String.valueOf(cropH));
            logKDK.log(10000000, "CombiMapController#handleKdkDualTerminal KDK cropping target: (%1, %2)",
                this.kdkPosX, this.kdkPosY);
            dm.setCropping(kdk, this.kombiTerminal, cropSrcX, cropSrcY, cropW, cropH, this.kdkPosX, this.kdkPosY, cropW, cropH);
            dm.setOpacity(kdk, this.kombiTerminal, this.kdkOpacity);
        }

        this.positionKdkBackgrounds(dm, layout);
        dm.setOpacity(visibleBg, this.kombiTerminal, visibleBgOpacity);
        dm.setOpacity(hiddenBg,  this.kombiTerminal, OPACITY_HIDDEN_FULL);
        dm.setKDKVisible(kdk, this.kombiTerminal);
    }

    /** MAIN-terminal (G24) KDK layout: position the maneuver and drive the fade animation. */
    private void applyKdkSingleTerminal(int kdk, int previousKdk, Layout layout) {
        logKDK.log(10000000, "CombiMapController#handleKDK kombiTerminal == HMITerminal.MAIN (G24)");
        int sportX = layout.getIntegerConstant(LC_SPORT_X);
        int popupX = layout.getIntegerConstant(LC_POPUP_X);
        int sportY = layout.getIntegerConstant(LC_SPORT_Y);
        int popupY = layout.getIntegerConstant(LC_POPUP_Y);

        if (kdk != NO_KDK) {
            boolean largeView = ((HMITerminalImpl) hmiService.getHMITerminal(0)).getViewSizeManager().getCurrentViewSize() == 2;
            this.kdkOpacity = OPACITY_HIDDEN_FULL;
            this.kdkPosX = largeView ? popupX : sportX;
            this.kdkPosY = largeView ? popupY : sportY;
            hmiService.getDisplayManager().setOpacity(kdk, this.kombiTerminal, this.kdkOpacity);
            hmiService.getDisplayManager().setPosition(kdk, this.kombiTerminal, this.kdkPosX, this.kdkPosY);
        }

        if (kdk != previousKdk) {
            if (kdk != NO_KDK) {
                ((IDisplayManagerKombiControl) hmiService.getDisplayManager()).setKDKVisible(kdk, this.kombiTerminal);
            }
            logKDK.log(10000000, "CombiMapController#handleKDK before starting kdk animation fade in: %1", kdk != NO_KDK);
            ((AnimationControllerMIB2High) hmiService.getHMITerminal(0).getIAnimationController())
                .startKDKAnimation(kdk != NO_KDK);
        }
    }

    /* ============================================================
     * Backgrounds / map placement
     * ============================================================ */

    private void positionKdkBackgrounds(IDisplayManager dm, Layout layout) {
        logKDK.log(10000000, "CombiMapController#connected layout: %1", layout.getClass().getName());
        int sportX = layout.getIntegerConstant(LC_SPORT_X);
        int sportY = layout.getIntegerConstant(LC_SPORT_Y);
        int popupX = layout.getIntegerConstant(LC_POPUP_X);
        int popupY = layout.getIntegerConstant(LC_POPUP_Y);
        logKDK.log(10000000, "CombiMapController#connected KDK background small stage: (%1, %2)", sportX, sportY);
        dm.setPosition(KDK_BG_SPORT, this.kombiTerminal, sportX, sportY);
        logKDK.log(10000000, "CombiMapController#connected KDK background large stage: (%1, %2)", popupX, popupY);
        dm.setPosition(KDK_BG_POPUP, this.kombiTerminal, popupX, popupY);
    }

    private void positionMap(IDisplayManager dm, Layout layout, boolean smallStage) {
        logKDK.log(10000000, "CombiMapController#positionMap layout: %1", layout.getClass().getName());
        int mapX = layout.getIntegerConstant(LC_MAP_X);
        int mapY = layout.getIntegerConstant(LC_MAP_Y);
        if (smallStage) {
            logKDK.log(10000000, "CombiMapController#positionMap adding a small stage offset from the layout");
            mapX += layout.getIntegerConstant(LC_SMALL_STAGE_MAP_DX);
            mapY += layout.getIntegerConstant(LC_SMALL_STAGE_MAP_DY);
        }
        logKDK.log(10000000, "CombiMapController#positionMap mapX: %1, mapY: %2", mapX, mapY);
        dm.setPosition(MAP_MAIN, this.kombiTerminal, mapX, mapY);
        dm.setPosition(MAP_ALT, this.kombiTerminal, mapX, mapY);
    }

    /* ============================================================
     * Frame rate + context switching
     * ============================================================ */

    private void updateFrameRate(ModelUpdateEvent modelupdateevent) {
        int hints = modelupdateevent.getHints();
        if ((hints & BITFIELD_FRAMERATE_FULL) != 0) {
            this.updateRate = FPS_FULL;
        } else if ((hints & BITFIELD_FRAMERATE_REDUCED) != 0) {
            this.updateRate = FPS_REDUCED;
        } else {
            this.updateRate = FPS_OFF;
        }

        int currentContext = hmiService.getDisplayManager().getCurrentContextID(this.kombiTerminal);
        if (currentContext == -1 || currentContext == CONTEXT_BLANK) {
            this.updateRate = FPS_OFF;
        }
        hmiService.getDisplayManager().setUpdateRate(this.kombiTerminal, this.updateRate);
    }

    private boolean isMapContext(int context) {
        return context >= MAP_CONTEXT_MIN && context <= MAP_CONTEXT_MAX;
    }

    public void switchToTargetContext() {
        /* CarPlay cluster pin: while CarPlay owns the cluster (ScreenModule.isConnected()), do NOT
         * switch terminal 1 to the stock kombi map.  On FPK the View button drives a model update ->
         * this controller -> switchContext(<stock map>), which is what pulled the cluster off ctx 80
         * ("View -> stock map").  Pin on isConnected() (true for the whole session from start()), NOT
         * clusterActive (set only after the first switch settles) — else a View press during the first
         * ~180ms switch slips the pin and strands the cluster on the stock map.  isConnected() clears
         * on disconnect, so the stock map returns then.  (Caller per probe stack: switchToTargetContext.) */
        if (this.kombiTerminal == CLUSTER_TERMINAL && com.luka.carplay.core.ScreenModule.isConnected()) {
            logDisplay.log(10000000, "CombiMapController#switchToTargetContext SKIPPED — CarPlay owns cluster (ctx 80)");
            return;
        }

        int target = this.getTargetContextID();
        if (this.kombiTerminal != 0 && !this.isMapContext(target)) {
            logDisplay.log(10000,
                "CombiMapController#switchToTargetContext not switching to context: %1 because it is no valid map context",
                target);
        } else {
            logDisplay.log(10000000, "CombiMapController#switchToTargetContext context: %1", target);
            hmiService.getDisplayManager().switchContext(target, this.kombiTerminal, this);
        }
    }

    /* ============================================================
     * Small helpers / accessors
     * ============================================================ */

    private static IDisplayManager displayManager(IHMIServiceEvo service) {
        return service != null ? service.getDisplayManager() : null;
    }

    private static Layout layoutOf(HMITerminalImpl terminal) {
        return terminal != null ? terminal.getLayout() : null;
    }

    private IDisplayManagerKombiControl kombiDisplayManager() {
        if (hmiService != null) {
            IDisplayManager dm = hmiService.getDisplayManager();
            if (dm instanceof IDisplayManagerKombiControl) {
                return (IDisplayManagerKombiControl) dm;
            }
        }
        return null;
    }

    public void setOpacityOnBackgroundLayers(float f, boolean flag) {
    }

    protected void positionDisplayables() {
    }

    public void setKombiTerminal(int i) {
        this.kombiTerminal = i;
    }

    public void setDisplayType(int i) {
        this.displayType = i;
    }
}
