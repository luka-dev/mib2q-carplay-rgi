package de.esolutions.hmi.widgets.audi.evo.high;

import com.luka.carplay.pdc.PdcSmallStageGuard;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.view.IPartialPopupController;
import de.audi.atip.hmi.view.IPartialPopupListener;
import de.audi.atip.model.ICoreSystemModelBank;
import de.audi.tghu.hmi.evo.DrawerAnimationListener;
import de.audi.tghu.hmi.evo.HMITerminalEvo;
import de.audi.tghu.hmi.evo.IDrawerFocusManagerEvo;
import de.audi.tghu.hmi.evo.IPartialPopupControllerEvo;
import de.audi.tghu.hmi.evo.IPopupManagerEvo;
import de.esolutions.fw.util.commons.Buffer;
import de.esolutions.hmi.widgets.audi.base.AbstractPartialPopupManager;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import de.esolutions.hmi.widgets.audi.base.HMITerminalImpl;
import de.esolutions.hmi.widgets.audi.base.ScreenMainArea;
import de.esolutions.hmi.widgets.audi.base.animation.AbstractAnimationController;
import de.esolutions.hmi.widgets.audi.base.eal.EALManager;
import de.esolutions.hmi.widgets.audi.evo.widgets.ContainerController;
import de.esolutions.hmi.widgets.audi.evo.widgets.EntertainmentDrawerController;
import de.esolutions.hmi.widgets.audi.evo.widgets.PartialPopupActivatorController;
import java.util.List;
import org.osgi.framework.BundleContext;

public class PartialPopupManagerEvoHigh extends AbstractPartialPopupManager implements DrawerAnimationListener {
    private static final float FIXED_PP_DRAWER_OPACITY = 0.75F;
    private static final int PP_SKIN_CHANGE = 95;
    protected IPopupManagerEvo popupManagerEvo;
    private boolean registeredAtDrawerFocusManager;
    private final ContainerController.MutableAnimationTransformation mainAreaTransform = new ContainerController.MutableAnimationTransformation();

    public PartialPopupManagerEvoHigh(
        HMITerminalImpl hmiterminalimpl, BundleContext bundlecontext, IFrameworkAccess iframeworkaccess, boolean flag
    ) {
        super(hmiterminalimpl, bundlecontext, iframeworkaccess);
    }

    public PartialPopupManagerEvoHigh(
        HMITerminalImpl hmiterminalimpl, BundleContext bundlecontext, IFrameworkAccess iframeworkaccess
    ) {
        this(hmiterminalimpl, bundlecontext, iframeworkaccess, true);
    }

    public String getPopupName(int i) {
        Buffer buffer = new Buffer();
        buffer.append(i);
        if (i == 52) {
            buffer.append(" (VolumePopup)");
        } else if (i == 62) {
            buffer.append(" (StatusBarG22)");
        } else if (i == 101) {
            buffer.append(" (StatusBarG24)");
        } else if (i == 2100008) {
            buffer.append(" (Car OPS)");
        } else if (i == 65) {
            buffer.append(" (DebugInfos)");
        } else if (i == 72) {
            buffer.append(" (Standby)");
        } else if (i == 85) {
            buffer.append(" (Presets)");
        } else if (i == 61) {
            buffer.append(" (TrafficAnnouncement)");
        } else if (i == 2100017) {
            buffer.append(" (SeatLeft)");
        } else if (i == 2100016) {
            buffer.append(" (SeatRight)");
        } else if (i == 75
            || i == 76
            || i == 78
            || i == 80
            || i == 96
            || i == 97
            || i == 98
            || i == 103
            || i == 105
            || i == 81) {
            buffer.append(" (UserHint)");
        } else if (i == 119) {
            buffer.append(" (Conversion Matrix)");
        }

        return buffer.toString();
    }

    protected boolean isSDSPartialPopup(IPartialPopupControllerEvo ipartialpopupcontrollerevo) {
        return false;
    }

    protected boolean shouldShowWithFixedWidth(IPartialPopupController ipartialpopupcontroller, int i) {
        return false;
    }

    protected int getPopupIdStatusLine() {
        return 62;
    }

    protected int getPopupIdVolume() {
        return 52;
    }

    protected int getPopupIdInvalid() {
        return -1;
    }

    protected IPartialPopupListener getActivatorListener(List list) {
        PartialPopupActivatorController partialpopupactivatorcontroller = null;

        for (int i = 0; i < list.size(); i++) {
            IPartialPopupListener ipartialpopuplistener = (IPartialPopupListener)list.get(i);
            if (ipartialpopuplistener instanceof PartialPopupActivatorController) {
                partialpopupactivatorcontroller = (PartialPopupActivatorController)ipartialpopuplistener;
            }
        }

        return partialpopupactivatorcontroller;
    }

    protected void repaintScreen() {
        if (this.currentConnectedScreen != null) {
            ((AbstractScreenWidget)this.currentConnectedScreen).doCheckedRepaint();
        } else if (((AbstractAnimationController)this.terminal.getIAnimationController()).isFirstScreenShown()) {
            LOGPOPUPS.log(
                10000, "PartialPopupManager#repaintScreen currentConnectedScreen is null (has not been set correctly)"
            );
        } else {
            LOGPOPUPS.log(
                10000000,
                "PartialPopupManager#repaintScreen first screen was not shown, repaint will be triggered by first screen"
            );
        }
    }

    protected boolean shouldCheckModelStatusOnExecutePopupAllowance(int i) {
        return true;
    }

    public int getHMIPrio(int i, int j) {
        if (this.popupManagerEvo == null) {
            this.popupManagerEvo = (IPopupManagerEvo)this.framework
                .getHMIService()
                .getPopupManager(this.terminal.getTerminalID());
        }

        return this.popupManagerEvo != null ? this.popupManagerEvo.getHMIInternalPrio(i, j) : 0;
    }

    private void updateDrawerTransformation(float[] afloat) {
        if (this.currentConnectedScreen != null) {
            ScreenMainArea screenmainarea = ((AbstractScreenWidget)this.currentConnectedScreen).getMainArea();
            this.mainAreaTransform.resetToIdentityTransformation();
            boolean flag = false;
            float f;
            if (screenmainarea != null && screenmainarea instanceof ContainerController) {
                ContainerController containercontroller = (ContainerController)screenmainarea;
                this.mainAreaTransform
                    .combine(containercontroller.getSelectionDrawerTransformation())
                    .combine(containercontroller.getOptionDrawerTransformation())
                    .combine(containercontroller.getEntertainmentDrawerTransformation());
                f = containercontroller.getSmallStageSelectionDrawerOpacity();
                flag = containercontroller.isPassOnOpacityToPartialPopups();
            } else {
                LOGPOPUPS.log(
                    1000000, "PartialPopupManager#updateDrawerTransforma current connected screen has no MainArea"
                );
                f = 1.0F;
            }

            EALManager ealmanager = (EALManager)this.terminal.getGUIManager();
            ealmanager.getPartialPopupsBackNode()
                .setPosition(this.mainAreaTransform.getTransX(), this.mainAreaTransform.getTransY(), 0.0F);
            ealmanager.getPartialPopupsBackNode()
                .setScale(this.mainAreaTransform.getScale(), this.mainAreaTransform.getScale(), 1.0F);
            float f1;
            if (flag) {
                f1 = this.mainAreaTransform.getOpacity();
            } else {
                float f2 = Math.max(afloat[2], afloat[0]);
                f2 = Math.max(afloat[4], f2);
                f1 = 1.0F - 0.25F * f2;
            }

            ealmanager.getPartialPopupsBackNode().setOpacity(f1 * f);
        }
    }

    /* Popup 62 owns the complete MMI footer, independently of APS content
     * 6001/6002. TerminalMode normally hides it via StatusBarStub renderStyle 2.
     * Preserve that presentation while pure OPS shares the CarPlay screen.
     * Use the stock hide path to clear animation/queue ownership as well.
     * Outside this guard, the next screen's normal show/hide request wins. */
    public boolean keepCarPlayStatusLineHidden() {
        return this.terminal != null
            && this.terminal.getTerminalID() == PdcSmallStageGuard.MAIN_TERMINAL
            && this.currentConnectedScreen != null
            && this.currentConnectedScreen.getID() == PdcSmallStageGuard.TERMINAL_MODE_SCREEN_ID
            && PdcSmallStageGuard.shouldKeepCarPlayScreen(this.currentConnectedScreen);
    }

    /* 62 and the entertainment drawer are one footer: StatusBarStubController
     * always toggles them together, and the drawer's glass plate sits in the
     * 62 gap, so hiding only 62 leaves a black block over CarPlay. */
    private void hideFooter() {
        super.hidePopup(62);
        IDrawerFocusManagerEvo focus = this.terminal.getDrawerFocusManager();
        Object drawer = focus != null ? focus.getEntertainmentDrawer() : null;
        if (drawer instanceof EntertainmentDrawerController) {
            ((EntertainmentDrawerController)drawer).getOpenCloseController().onDrawerVisibiltyChange(false);
        }
    }

    protected int doShowPopup(IPartialPopupControllerEvo popup, int style) {
        // Also cover stock queue replay after a fullscreen popup, which bypasses
        // public showPopup(). Never suppress another popup or the right OPS.
        if ((popup.getID() == 62 || popup.getID() == PdcSmallStageGuard.PURE_OPS_POPUP_ID)
            && this.keepCarPlayStatusLineHidden()) {
            if (popup.getID() == 62) return 2;
            this.hideFooter();
        }
        return super.doShowPopup(popup, style);
    }

    public int showPopup(int i) {
        if ((i == 62 || i == PdcSmallStageGuard.PURE_OPS_POPUP_ID)
            && this.keepCarPlayStatusLineHidden()) {
            this.hideFooter();
            if (i == 62) return 2;
        }
        int j = 3;
        if (i == 95) {
            ((HMITerminalEvo)this.terminal).setSkin(1);
            j = 1;
        } else {
            if (!this.registeredAtDrawerFocusManager) {
                IDrawerFocusManagerEvo idrawerfocusmanagerevo = this.terminal.getDrawerFocusManager();
                if (idrawerfocusmanagerevo != null) {
                    idrawerfocusmanagerevo.registerDrawerAnimationListener(this);
                    this.registeredAtDrawerFocusManager = true;
                }
            }

            j = super.showPopup(i);
        }

        return j;
    }

    public int hidePopup(int i) {
        if (i == 95) {
            ((HMITerminalEvo)this.terminal).setSkin(0);
            return 2;
        } else {
            return super.hidePopup(i);
        }
    }

    public void initializeDrawerAnimation(float[] afloat, float[] afloat1) {
        this.updateDrawerTransformation(afloat);
    }

    public void drawerAnimationTargetChanged(float[] afloat, float[] afloat1, int i) {
    }

    public void setDrawerAnimation(float[] afloat, float[] afloat1, int i) {
        this.updateDrawerTransformation(afloat);
    }

    public void drawerAnimationFinished(float[] afloat, float[] afloat1, int i) {
        this.updateDrawerTransformation(afloat);
    }

    public int getDrawerAnimationMask() {
        return ICoreSystemModelBank.TV_TUNER_AVAILABLE_CHOICE;
    }
}
