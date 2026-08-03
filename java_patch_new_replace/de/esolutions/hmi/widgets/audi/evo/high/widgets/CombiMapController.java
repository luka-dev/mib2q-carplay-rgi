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
import de.esolutions.hmi.widgets.audi.base.IWidgetLogChannel;
import de.esolutions.hmi.widgets.audi.base.InitializationContext;
import de.esolutions.hmi.widgets.audi.base.Layout;
import de.esolutions.hmi.widgets.audi.base.widgets.IRenderer;
import de.esolutions.hmi.widgets.audi.evo.DisplayControllerEvo;
import de.esolutions.hmi.widgets.audi.evo.IAnimationTypesEvo;
import de.esolutions.hmi.widgets.audi.evo.high.AnimationControllerMIB2High;

public class CombiMapController extends DisplayControllerEvo implements NaviMoKoKDKConstants {
    private static final boolean SMALL_STAGE_FLAG_DEFAULT = false;
    private int kombiTerminal = 1;
    private int displayType;
    private int updateRate = 10;
    private int kdkDisplayable = -1;
    private int kdkPosX = 0;
    private int kdkPosY = 0;
    private int kdkOpacity = 0;

    public IRenderer getRenderer() {
        return null;
    }

    public void connected(InitializationContext initializationcontext) {
        super.connected(initializationcontext);
        if (this.kombiTerminal == 1) {
            IDisplayManager idisplaymanager = this.getDisplayManager(hmiService);
            Layout layout = this.getLayout(this.terminal);
            if (idisplaymanager != null && layout != null) {
                idisplaymanager.setDisplayType(1, this.displayType);
                this.positionKDKBackgrounds(idisplaymanager, layout);
                this.positionMap(idisplaymanager, layout, false);
            }
        }
    }

    private IDisplayManager getDisplayManager(IHMIServiceEvo ihmiserviceevo) {
        IDisplayManager idisplaymanager = null;
        if (ihmiserviceevo != null) {
            idisplaymanager = ihmiserviceevo.getDisplayManager();
        }

        return idisplaymanager;
    }

    private Layout getLayout(HMITerminalImpl hmiterminalimpl) {
        Layout layout = null;
        if (hmiterminalimpl != null) {
            layout = hmiterminalimpl.getLayout();
        }

        return layout;
    }

    private void positionKDKBackgrounds(IDisplayManager idisplaymanager, Layout layout) {
        logKDK.log(10000000, "CombiMapController#connected layout: %1", layout.getClass().getName());
        int i = layout.getIntegerConstant(58);
        int j = layout.getIntegerConstant(59);
        int k = layout.getIntegerConstant(60);
        int l = layout.getIntegerConstant(61);
        logKDK.log(10000000, "CombiMapController#connected KDK background small stage: (%1, %2)", i, j);
        idisplaymanager.setPosition(101, this.kombiTerminal, i, j);
        logKDK.log(10000000, "CombiMapController#connected KDK background large stage: (%1, %2)", k, l);
        idisplaymanager.setPosition(102, this.kombiTerminal, k, l);
    }

    private void positionMap(IDisplayManager idisplaymanager, Layout layout, boolean flag) {
        logKDK.log(10000000, "CombiMapController#positionMap layout: %1", layout.getClass().getName());
        int i = layout.getIntegerConstant(IAnimationTypesEvo.TYPE_3D_DRIVESELECT_GRID_ANIMATION);
        int j = layout.getIntegerConstant(IAnimationTypesEvo.NUM_ANIMATION_TYPES);
        if (flag) {
            logKDK.log(10000000, "CombiMapController#positionMap adding a small stage offset from the layout");
            i += layout.getIntegerConstant(80);
            j += layout.getIntegerConstant(81);
        }

        logKDK.log(10000000, "CombiMapController#positionMap mapX: %1, mapY: %2", i, j);
        idisplaymanager.setPosition(33, this.kombiTerminal, i, j);
        idisplaymanager.setPosition(58, this.kombiTerminal, i, j);
    }

    public void processModelUpdateEvent(ModelUpdateEvent modelupdateevent) {
        logKDK.log(1000000, "CombiMapController#processModelUpdateEvent %1", modelupdateevent);
        if (modelupdateevent.getModelId() == ICoreNaviModelBank.NAV_VIEW_SIZE_CHOICE) {
            if (this.kombiTerminal != 0) {
                IDisplayManager idisplaymanager = this.getDisplayManager(hmiService);
                Layout layout = this.getLayout(this.terminal);
                if (idisplaymanager != null && layout != null && hmiService != null) {
                    HMIModel hmimodel = hmiService.getModel(ICoreNaviModelBank.NAV_VIEW_SIZE_CHOICE);
                    boolean flag = false;
                    if (hmimodel instanceof ChoiceModelGUI) {
                        flag = ((ChoiceModelGUI)hmimodel).getValue() == 1;
                    }

                    this.positionMap(idisplaymanager, layout, flag);
                } else {
                    logKDK.log(100000, "CombiMapController#processModelUpdateEvent environment not fully initialized");
                }
            }
        } else if (this.kombiTerminal != 0) {
            if (framework.getSysConst(541) == 2) {
                this.handleKDK(modelupdateevent);
                this.switchToTargetContext();
            } else {
                this.switchToTargetContext();
                this.updateFrameRate(modelupdateevent);
            }
        } else {
            this.handleKDK(modelupdateevent);
        }
    }

    private void handleKDK(ModelUpdateEvent modelupdateevent) {
        IDisplayManagerKombiControl idisplaymanagerkombicontrol = this.getDisplayManager();
        if (idisplaymanagerkombicontrol == null) {
            IWidgetLogChannel.logKDK
                .log(
                    100000,
                    "CombiMapController#handleKDK failed to retrieve the display manager; ignoring the invocation"
                );
        } else {
            int i = this.updateKdkDisplayable(modelupdateevent);
            Layout layout = this.terminal.getLayout();
            if (this.kombiTerminal != 0) {
                this.handleKdkDualTerminal(idisplaymanagerkombicontrol, this.kdkDisplayable, modelupdateevent, layout);
            } else {
                this.handleKdkSingleTerminal(this.kdkDisplayable, i, layout);
            }
            /* CarPlay cluster plane geometry (98 maneuver + 101/102 backing) — owned entirely by
             * ClusterLayerController (hardcoded geometry + /tmp override + nav-active gate). */
            com.luka.carplay.cluster.ClusterLayerController.apply(
                    idisplaymanagerkombicontrol, this.kombiTerminal, (modelupdateevent.getHints() & 8) == 0);
        }
    }

    private IDisplayManagerKombiControl getDisplayManager() {
        IDisplayManagerKombiControl idisplaymanagerkombicontrol = null;
        if (hmiService != null) {
            IDisplayManager idisplaymanager = hmiService.getDisplayManager();
            if (idisplaymanager instanceof IDisplayManagerKombiControl) {
                idisplaymanagerkombicontrol = (IDisplayManagerKombiControl)idisplaymanager;
            }
        }

        return idisplaymanagerkombicontrol;
    }

    private int updateKdkDisplayable(ModelUpdateEvent modelupdateevent) {
        int i = this.kdkDisplayable;
        this.kdkDisplayable = this.getKdkDisplayable(modelupdateevent);
        logKDK.log(
            10000000,
            "CombiMapController#updateKdkDisplayable received model update, previous visible kdk: %1 new visible kdk: %2",
            i,
            this.kdkDisplayable
        );
        /* NOTE: nav-active is NOT derived here from the stock KDK-visible bit (kdk==20) — that only
         * says "the stock cluster is drawing a KDK panel", not "our CarPlay RGI is active".  The
         * authoritative flip lives in RouteGuidance (rgActive, == the BAP nav-active message), which
         * calls AltScreenModule.setNavActive() directly. */
        return i;
    }

    private int getKdkDisplayable(ModelUpdateEvent modelupdateevent) {
        byte b0 = -1;
        if ((modelupdateevent.getHints() & 4) > 0) {
            b0 = 20;
        }

        return b0;
    }

    private void handleKdkDualTerminal(
        IDisplayManagerKombiControl idisplaymanagerkombicontrol,
        int i,
        ModelUpdateEvent modelupdateevent,
        Layout layout
    ) {
        byte b0 = 102;
        int j = 0;
        byte b1 = 101;
        byte b2 = 0;
        if (i != -1) {
            this.kdkOpacity = 0;
            if ((modelupdateevent.getHints() & 16) > 0) {
                this.kdkOpacity = 100;
            }

            j = this.kdkOpacity;
            logKDK.log(10000000, "CombiMapController#handleKdkDualTerminal layout: %1", layout.getClass().getName());
            int k = layout.getIntegerConstant(118);
            int l = layout.getIntegerConstant(119);
            int i1 = layout.getIntegerConstant(120);
            int j1 = layout.getIntegerConstant(121);
            this.kdkPosX = layout.getIntegerConstant(60);
            this.kdkPosY = layout.getIntegerConstant(61);
            if ((modelupdateevent.getHints() & 8) > 0) {
                b0 = 101;
                b1 = 102;
                k = layout.getIntegerConstant(122);
                l = layout.getIntegerConstant(123);
                i1 = layout.getIntegerConstant(124);
                j1 = layout.getIntegerConstant(125);
                this.kdkPosX = layout.getIntegerConstant(58);
                this.kdkPosY = layout.getIntegerConstant(59);
            }

            logKDK.log(
                10000000,
                "CombiMapController#handleKdkDualTerminal KDK cropping source: (%1, %2), size: %3 x %4",
                String.valueOf(k),
                String.valueOf(l),
                String.valueOf(i1),
                String.valueOf(j1)
            );
            logKDK.log(
                10000000,
                "CombiMapController#handleKdkDualTerminal KDK cropping target: (%1, %2)",
                this.kdkPosX,
                this.kdkPosY
            );
            idisplaymanagerkombicontrol.setCropping(
                i, this.kombiTerminal, k, l, i1, j1, this.kdkPosX, this.kdkPosY, i1, j1
            );
            logKDK.log(
                10000000, "CombiMapController#handleKdkDualTerminal setting the KDK's opacity to %1", this.kdkOpacity
            );
            idisplaymanagerkombicontrol.setOpacity(i, this.kombiTerminal, this.kdkOpacity);
            /* NOTE: 98's geometry/opacity is applied by ClusterLayerController (called from handleKDK
             * after this method) — no mirror here, it would only be overwritten. */
        }

        this.positionKDKBackgrounds(idisplaymanagerkombicontrol, layout);
        logKDK.log(
            10000000,
            "CombiMapController#handleKdkDualTerminal setting the primary KDK backround's opacity to %1, the secondary background's opacity to %2",
            j,
            b2
        );
        idisplaymanagerkombicontrol.setOpacity(b0, this.kombiTerminal, j);
        idisplaymanagerkombicontrol.setOpacity(b1, this.kombiTerminal, b2);
        idisplaymanagerkombicontrol.setKDKVisible(i, this.kombiTerminal);
    }

    private void handleKdkSingleTerminal(int i, int j, Layout layout) {
        logKDK.log(10000000, "CombiMapController#handleKDK kombiTerminal == HMITerminal.MAIN (G24)");
        int k = layout.getIntegerConstant(58);
        int l = layout.getIntegerConstant(60);
        int i1 = layout.getIntegerConstant(59);
        int j1 = layout.getIntegerConstant(61);
        if (i != -1) {
            boolean flag = ((HMITerminalImpl)hmiService.getHMITerminal(0)).getViewSizeManager().getCurrentViewSize()
                == 2;
            this.kdkOpacity = 0;
            this.kdkPosX = flag ? l : k;
            this.kdkPosY = flag ? j1 : i1;
            hmiService.getDisplayManager().setOpacity(i, this.kombiTerminal, this.kdkOpacity);
            hmiService.getDisplayManager().setPosition(i, this.kombiTerminal, this.kdkPosX, this.kdkPosY);
            /* NOTE: 98 handled by ClusterLayerController (see handleKDK) — no mirror here. */
        }

        if (i != j) {
            if (i != -1) {
                ((IDisplayManagerKombiControl)hmiService.getDisplayManager()).setKDKVisible(i, this.kombiTerminal);
            }

            logKDK.log(10000000, "CombiMapController#handleKDK before starting kdk animation fade in: %1", i != -1);
            ((AnimationControllerMIB2High)hmiService.getHMITerminal(0).getIAnimationController())
                .startKDKAnimation(i != -1);
        }
    }

    private void updateFrameRate(ModelUpdateEvent modelupdateevent) {
        int i = modelupdateevent.getHints();
        if ((i & 2) > 0) {
            this.updateRate = 10;
        } else if ((i & 1) > 0) {
            this.updateRate = 1;
        } else {
            this.updateRate = 0;
        }

        int j = hmiService.getDisplayManager().getCurrentContextID(this.kombiTerminal);
        if (j == -1 || j == 75) {
            this.updateRate = 0;
        }

        hmiService.getDisplayManager().setUpdateRate(this.kombiTerminal, this.updateRate);
    }

    private boolean isMapContext(int i) {
        boolean flag = false;
        switch (i) {
            case 72:
            case 73:
            case 74:
            case 75:
            case 76:
            case 77:
                flag = true;
                break;
            default:
                flag = false;
        }

        return flag;
    }

    public void switchToTargetContext() {
        /* CarPlay cluster pin: while CarPlay owns the cluster (AltScreenModule.clusterActive), do NOT
         * switch terminal 1 to the stock kombi map.  On FPK the View button drives a model update →
         * this controller → switchContext(<stock map>), which is what pulled the cluster off our ctx 80
         * ("View → stock map").  AltScreenModule clears clusterActive on disconnect, so the normal stock
         * map returns then.  This is the exact caller (probe stack: CombiMapController.switchToTargetContext). */
        if (this.kombiTerminal == 1 && com.luka.carplay.core.AltScreenModule.isClusterActive()) {
            logDisplay.log(10000000, "CombiMapController#switchToTargetContext SKIPPED — CarPlay owns cluster (ctx 80)");
            return;
        }
        if (this.kombiTerminal != 0 && !this.isMapContext(this.getTargetContextID())) {
            logDisplay.log(
                10000,
                "CombiMapController#switchToTargetContext not switching to context: %1 because it is no valid map context",
                this.getTargetContextID()
            );
        } else {
            logDisplay.log(10000000, "CombiMapController#switchToTargetContext context: %1", this.getTargetContextID());
            hmiService.getDisplayManager().switchContext(this.getTargetContextID(), this.kombiTerminal, this);
        }
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
