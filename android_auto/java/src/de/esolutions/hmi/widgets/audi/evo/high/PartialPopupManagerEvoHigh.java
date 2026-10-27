/*
 * MU0918 stock PartialPopupManagerEvoHigh (extends the MU0918 PartialPopupManager, not MU1316's
 * AbstractPartialPopupManager), rebuilt from the lsd.jar decompile with the byte-swapped int/float
 * constants restored (lsd.jxe), plus luka-dev's footer policy for the SQ5 Android Auto PDC port:
 *
 *   While PdcSmallStageGuard keeps the TerminalMode screen 3200000 beside the right OPS popup
 *   2100008 on terminal 0, the MMI status line (partial popup 62) and the entertainment drawer
 *   that stock pairs with it are hidden when OPS opens, and a repeat showPopup(62) or a queue
 *   replay of 62 answers HIDDEN (2).  No other popup, terminal or screen is affected; OPS itself
 *   is never suppressed.
 */
package de.esolutions.hmi.widgets.audi.evo.high;

import com.sq5.aa.pdc.PdcSmallStageGuard;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.view.IPartialPopupController;
import de.audi.atip.hmi.view.IPartialPopupListener;
import de.audi.tghu.hmi.evo.DrawerAnimationListener;
import de.audi.tghu.hmi.evo.HMITerminalEvo;
import de.audi.tghu.hmi.evo.IDrawerFocusManagerEvo;
import de.audi.tghu.hmi.evo.IPartialPopupControllerEvo;
import de.audi.tghu.hmi.evo.IPopupManagerEvo;
import de.esolutions.fw.util.commons.Buffer;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import de.esolutions.hmi.widgets.audi.base.HMITerminalImpl;
import de.esolutions.hmi.widgets.audi.base.PartialPopupManager;
import de.esolutions.hmi.widgets.audi.base.ScreenMainArea;
import de.esolutions.hmi.widgets.audi.base.animation.AbstractAnimationController;
import de.esolutions.hmi.widgets.audi.base.eal.EALManager;
import de.esolutions.hmi.widgets.audi.evo.widgets.ContainerController;
import de.esolutions.hmi.widgets.audi.evo.widgets.EntertainmentDrawerController;
import de.esolutions.hmi.widgets.audi.evo.widgets.PartialPopupActivatorController;
import java.util.List;
import org.osgi.framework.BundleContext;

public class PartialPopupManagerEvoHigh
extends PartialPopupManager
implements DrawerAnimationListener {
    private static final float FIXED_PP_DRAWER_OPACITY = 0.75f;
    private static final int PP_SKIN_CHANGE = 95;
    protected IPopupManagerEvo popupManagerEvo;
    private boolean registeredAtDrawerFocusManager;
    private final ContainerController.MutableAnimationTransformation mainAreaTransform = new ContainerController.MutableAnimationTransformation();

    public PartialPopupManagerEvoHigh(HMITerminalImpl hMITerminalImpl, BundleContext bundleContext, IFrameworkAccess iFrameworkAccess, boolean bl) {
        super(hMITerminalImpl, bundleContext, iFrameworkAccess);
    }

    public PartialPopupManagerEvoHigh(HMITerminalImpl hMITerminalImpl, BundleContext bundleContext, IFrameworkAccess iFrameworkAccess) {
        this(hMITerminalImpl, bundleContext, iFrameworkAccess, true);
    }

    public String getPopupName(int n) {
        Buffer buffer = new Buffer();
        buffer.append(n);
        if (n == 52) {
            buffer.append(" (VolumePopup)");
        } else if (n == 62) {
            buffer.append(" (StatusBarG22)");
        } else if (n == 101) {
            buffer.append(" (StatusBarG24)");
        } else if (n == 2100008) {
            buffer.append(" (Car OPS)");
        } else if (n == 65) {
            buffer.append(" (DebugInfos)");
        } else if (n == 72) {
            buffer.append(" (Standby)");
        } else if (n == 85) {
            buffer.append(" (Presets)");
        } else if (n == 61) {
            buffer.append(" (TrafficAnnouncement)");
        } else if (n == 2100017) {
            buffer.append(" (SeatLeft)");
        } else if (n == 2100016) {
            buffer.append(" (SeatRight)");
        } else if (n == 75 || n == 76 || n == 78 || n == 80 || n == 96 || n == 97 || n == 98 || n == 103 || n == 105 || n == 81) {
            buffer.append(" (UserHint)");
        } else if (n == 119) {
            buffer.append(" (Conversion Matrix)");
        }
        return buffer.toString();
    }

    protected boolean isSDSPartialPopup(IPartialPopupControllerEvo iPartialPopupControllerEvo) {
        return false;
    }

    protected boolean shouldShowWithFixedWidth(IPartialPopupController iPartialPopupController, int n) {
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
        PartialPopupActivatorController partialPopupActivatorController = null;
        for (int i2 = 0; i2 < list.size(); ++i2) {
            IPartialPopupListener iPartialPopupListener = (IPartialPopupListener)list.get(i2);
            if (!(iPartialPopupListener instanceof PartialPopupActivatorController)) continue;
            partialPopupActivatorController = (PartialPopupActivatorController)iPartialPopupListener;
        }
        return partialPopupActivatorController;
    }

    protected void repaintScreen() {
        if (this.currentConnectedScreen != null) {
            ((AbstractScreenWidget)this.currentConnectedScreen).doCheckedRepaint();
        } else if (((AbstractAnimationController)this.terminal.getIAnimationController()).isFirstScreenShown()) {
            LOGPOPUPS.log(10000, "PartialPopupManager#repaintScreen currentConnectedScreen is null (has not been set correctly)");
        } else {
            LOGPOPUPS.log(10000000, "PartialPopupManager#repaintScreen first screen was not shown, repaint will be triggered by first screen");
        }
    }

    protected boolean shouldCheckModelStatusOnExecutePopupAllowance(int n) {
        return true;
    }

    public int getHMIPrio(int n, int n2) {
        if (this.popupManagerEvo == null) {
            this.popupManagerEvo = (IPopupManagerEvo)((Object)this.framework.getHMIService().getPopupManager(this.terminal.getTerminalID()));
        }
        if (this.popupManagerEvo != null) {
            return this.popupManagerEvo.getHMIInternalPrio(n, n2);
        }
        return 0;
    }

    private void updateDrawerTransformation(float[] fArray) {
        float f2;
        float f3;
        if (this.currentConnectedScreen == null) {
            return;
        }
        ScreenMainArea screenMainArea = ((AbstractScreenWidget)this.currentConnectedScreen).getMainArea();
        this.mainAreaTransform.resetToIdentityTransformation();
        boolean bl = false;
        if (screenMainArea == null || !(screenMainArea instanceof ContainerController)) {
            LOGPOPUPS.log(1000000, "PartialPopupManager#updateDrawerTransforma current connected screen has no MainArea");
            f3 = 1.0f;
        } else {
            ContainerController containerController = (ContainerController)screenMainArea;
            this.mainAreaTransform.combine(containerController.getSelectionDrawerTransformation()).combine(containerController.getOptionDrawerTransformation()).combine(containerController.getEntertainmentDrawerTransformation());
            f3 = containerController.getSmallStageSelectionDrawerOpacity();
            bl = containerController.isPassOnOpacityToPartialPopups();
        }
        EALManager eALManager = (EALManager)this.terminal.getGUIManager();
        eALManager.getPartialPopupsBackNode().setPosition(this.mainAreaTransform.getTransX(), this.mainAreaTransform.getTransY(), 0.0f);
        eALManager.getPartialPopupsBackNode().setScale(this.mainAreaTransform.getScale(), this.mainAreaTransform.getScale(), 1.0f);
        if (bl) {
            f2 = this.mainAreaTransform.getOpacity();
        } else {
            float f4 = Math.max(fArray[2], fArray[0]);
            f4 = Math.max(fArray[4], f4);
            f2 = 1.0f - 0.25f * f4;
        }
        eALManager.getPartialPopupsBackNode().setOpacity(f2 * f3);
    }

    /* SQ5 PDC (luka): popup 62 owns the complete MMI footer.  TerminalMode normally hides it via
     * StatusBarStub renderStyle 2; preserve that while pure OPS shares the projection screen.
     * Outside the guard the next screen's normal show/hide request wins. */
    public boolean keepProjectionStatusLineHidden() {
        try {
            return this.terminal != null
                && this.terminal.getTerminalID() == PdcSmallStageGuard.MAIN_TERMINAL
                && this.currentConnectedScreen != null
                && this.currentConnectedScreen.getID() == PdcSmallStageGuard.TERMINAL_MODE_SCREEN_ID
                && PdcSmallStageGuard.shouldKeepProjectionScreen(this.currentConnectedScreen);
        } catch (Throwable t) {
            return false;
        }
    }

    /* 62 and the entertainment drawer are one footer: stock StatusBarStubController toggles them
     * together, so hiding only 62 would leave the drawer's glass plate over the projection. */
    private void hideFooter() {
        super.hidePopup(62);
        try {
            IDrawerFocusManagerEvo iDrawerFocusManagerEvo = this.terminal.getDrawerFocusManager();
            Object object = iDrawerFocusManagerEvo != null ? iDrawerFocusManagerEvo.getEntertainmentDrawer() : null;
            if (object instanceof EntertainmentDrawerController) {
                ((EntertainmentDrawerController)object).getOpenCloseController().onDrawerVisibiltyChange(false);
            }
        } catch (Throwable t) {
            /* cosmetic only */
        }
    }

    protected int doShowPopup(IPartialPopupControllerEvo iPartialPopupControllerEvo, int n) {
        // SQ5 PDC: also covers the stock queue replay after a fullscreen popup, which bypasses
        // public showPopup().  Never suppresses another popup or the right OPS.
        int id = iPartialPopupControllerEvo.getID();
        if ((id == 62 || id == PdcSmallStageGuard.PURE_OPS_POPUP_ID) && this.keepProjectionStatusLineHidden()) {
            if (id == 62) {
                return 2;
            }
            this.hideFooter();
        }
        return super.doShowPopup(iPartialPopupControllerEvo, n);
    }

    public int showPopup(int n) {
        if ((n == 62 || n == PdcSmallStageGuard.PURE_OPS_POPUP_ID) && this.keepProjectionStatusLineHidden()) {
            this.hideFooter();
            if (n == 62) {
                return 2;
            }
        }
        int n2 = 3;
        if (n == 95) {
            ((HMITerminalEvo)((Object)this.terminal)).setSkin(1);
            n2 = 1;
        } else {
            IDrawerFocusManagerEvo iDrawerFocusManagerEvo;
            if (!this.registeredAtDrawerFocusManager && (iDrawerFocusManagerEvo = this.terminal.getDrawerFocusManager()) != null) {
                iDrawerFocusManagerEvo.registerDrawerAnimationListener(this);
                this.registeredAtDrawerFocusManager = true;
            }
            n2 = super.showPopup(n);
        }
        return n2;
    }

    public int hidePopup(int n) {
        if (n == 95) {
            ((HMITerminalEvo)((Object)this.terminal)).setSkin(0);
            return 2;
        }
        return super.hidePopup(n);
    }

    public void initializeDrawerAnimation(float[] fArray, float[] fArray2) {
        this.updateDrawerTransformation(fArray);
    }

    public void drawerAnimationTargetChanged(float[] fArray, float[] fArray2, int n) {
    }

    public void setDrawerAnimation(float[] fArray, float[] fArray2, int n) {
        this.updateDrawerTransformation(fArray);
    }

    public void drawerAnimationFinished(float[] fArray, float[] fArray2, int n) {
        this.updateDrawerTransformation(fArray);
    }

    public int getDrawerAnimationMask() {
        return 4117;
    }
}
