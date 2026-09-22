package de.esolutions.hmi.widgets.audi.evo.widgets;

import de.audi.atip.hmi.event.ModelUpdateEvent;
import de.audi.atip.hmi.modelaccess.ChoiceModelGUI;
import de.audi.atip.hmi.view.IPartialPopupController;
import de.audi.atip.hmi.view.IPartialPopupManager;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import de.esolutions.hmi.widgets.audi.base.IWidgetLogChannel;
import de.esolutions.hmi.widgets.audi.base.ScreenMainArea;
import de.esolutions.hmi.widgets.audi.base.widgets.AbstractWidgetController;
import de.esolutions.hmi.widgets.audi.base.widgets.IRenderer;
import de.esolutions.hmi.widgets.audi.evo.ScreenWidgetEVO;
import de.esolutions.hmi.widgets.audi.evo.high.PartialPopupManagerEvoHigh;

public class StatusBarStubController extends AbstractWidgetController {
    private int bluetoothState;
    private int adbImportState;
    private int adb1ImportState;
    private int adb2ImportState;
    private int googleProgressState;
    private int googleTrademarkState;
    private int trafficSourceState;
    private int mediaImportState;
    private int mediaMixState;
    private int mediaRepeatState;
    private int gracenoteState;
    private int roamingState;
    private int providerLogoState;
    private int providerNameState;
    private int wirelessChargingState;
    private int providerLogoConnectState;
    private int smsStorageFullState;
    private int bluetoothMediaState;
    private int dragonLogoState;
    private int selfLearningNavigationState;
    private int bluetoothMediaOnState;
    private int asiaVICSState;
    private int asiaTPEGState;
    private int asiaTTSState;
    public static final int ICON_STATE_IGNORE = 0;
    public static final int ICON_STATE_SHOW = 1;
    public static final int ICON_STATE_HIDE = 2;
    protected int renderStyle = 0;
    protected int hmiContext = -1;
    private StatusBarG24Controller statusbarG24;

    protected void initializeWidget() {
        super.initializeWidget();
        this.updateStatusBar();
    }

    private void updateStatusBar() {
        int i = this.getPPID();
        if (this.renderStyle == 2) {
            this.terminal.getPartialPopupManager().hidePopup(i);
            this.setEntertainmentDrawerVisible(false);
        } else if (this.renderStyle == 3) {
            this.hmiContext = -1;
            this.terminal.getPartialPopupManager().showPopup(i);
            this.setEntertainmentDrawerVisible(true);
        } else {
            this.terminal.getPartialPopupManager().showPopup(i);
            this.setEntertainmentDrawerVisible(true);
            if (this.model != null) {
                int j = ((ChoiceModelGUI)this.model).getValue();
                switch (j) {
                    case 0:
                        this.hmiContext = 0;
                        break;
                    case 1:
                        this.hmiContext = 1;
                        break;
                    case 2:
                        this.hmiContext = 2;
                        break;
                    case 3:
                        this.hmiContext = 3;
                        break;
                    case 4:
                        this.hmiContext = 4;
                        break;
                    case 5:
                        this.hmiContext = 5;
                        break;
                    case 6:
                        this.hmiContext = 6;
                        break;
                    case 7:
                        this.hmiContext = 7;
                        break;
                    case 8:
                        this.hmiContext = 8;
                        break;
                    case 9:
                        this.hmiContext = 9;
                        break;
                    default:
                        this.hmiContext = -1;
                }
            } else {
                this.hmiContext = -1;
            }
        }

        if (this.terminal.getFramework().getKombiType() == 4) {
            this.updateStatusbarG24(this.hmiContext);
        } else {
            this.updateStatusbarHigh(this.hmiContext);
        }
    }

    private void setEntertainmentDrawerVisible(boolean flag) {
        if (this.terminal != null) {
            // Stock pairs the drawer with footer popup 62. While side OPS keeps
            // CarPlay on screen, 62 stays hidden, so its drawer (the black glass
            // plate in the footer gap) must stay hidden too.
            IPartialPopupManager ppm = this.terminal.getPartialPopupManager();
            if (flag && ppm instanceof PartialPopupManagerEvoHigh
                && ((PartialPopupManagerEvoHigh)ppm).keepCarPlayStatusLineHidden()) {
                flag = false;
            }
            Object object = this.terminal.getDrawerFocusManager().getEntertainmentDrawer();
            if (object instanceof EntertainmentDrawerController) {
                EntertainmentDrawerOpenCloseController entertainmentdraweropenclosecontroller = ((EntertainmentDrawerController)object)
                    .getOpenCloseController();
                entertainmentdraweropenclosecontroller.onDrawerVisibiltyChange(flag);
            } else if (object == null) {
                IWidgetLogChannel.logEntertainmentDrawerEvents
                    .log(
                        100000,
                        "StatusBarStubController#setEntertainmentDrawerVisible entertainmentDrawer is not initialized yet, store visibility at drawerfocusmanager. Will be set when entertainment drawer is activated -> visible=%1",
                        flag
                    );
                this.terminal.getDrawerFocusManager().setEarlyEntertainmentDrawerVisibility(flag);
            }
        }
    }

    private int getPPID() {
        return this.isKombiType() ? 101 : 62;
    }

    private void updateStatusbarHigh(int i) {
        IPartialPopupController ipartialpopupcontroller = this.terminal.getPartialPopupManagerEvo().getPartialPopup(62);
        if (ipartialpopupcontroller != null) {
            AbstractStatusBarController abstractstatusbarcontroller = (AbstractStatusBarController)((AbstractPartialPopupController)ipartialpopupcontroller)
                .getChild(0);
            if (this.isStatusbarValid(abstractstatusbarcontroller)) {
                abstractstatusbarcontroller.setCurrentHMIContext(i);
                abstractstatusbarcontroller.setIconState(
                    this.bluetoothState,
                    this.adbImportState,
                    this.adb1ImportState,
                    this.adb2ImportState,
                    this.googleProgressState,
                    this.googleTrademarkState,
                    this.trafficSourceState,
                    this.mediaImportState,
                    this.mediaMixState,
                    this.mediaRepeatState,
                    this.gracenoteState,
                    this.roamingState,
                    this.providerLogoState,
                    this.providerNameState,
                    this.wirelessChargingState,
                    this.providerLogoConnectState,
                    this.smsStorageFullState,
                    this.bluetoothMediaState,
                    this.asiaVICSState,
                    this.asiaTPEGState,
                    this.asiaTTSState,
                    this.dragonLogoState,
                    this.selfLearningNavigationState,
                    this.bluetoothMediaOnState
                );
            }
        }
    }

    private void updateStatusbarG24(int i) {
        PartialPopupController partialpopupcontroller = (PartialPopupController)this.terminal
            .getPartialPopupManagerEvo()
            .getPartialPopup(101);
        if (partialpopupcontroller != null) {
            this.statusbarG24 = (StatusBarG24Controller)partialpopupcontroller.getChild(0);
            if (this.isStatusbarValid(this.statusbarG24)) {
                this.statusbarG24.setScreen((AbstractScreenWidget)this.getInitContext().getScreen());
                this.statusbarG24.setCurrentHMIContext(i);
                this.statusbarG24
                    .setIconState(
                        this.bluetoothState,
                        this.adbImportState,
                        this.adb1ImportState,
                        this.adb2ImportState,
                        this.googleProgressState,
                        this.googleTrademarkState,
                        this.trafficSourceState,
                        this.mediaImportState,
                        this.mediaMixState,
                        this.mediaRepeatState,
                        this.gracenoteState,
                        this.roamingState,
                        this.providerLogoState,
                        this.providerNameState,
                        this.wirelessChargingState,
                        this.providerLogoConnectState,
                        this.smsStorageFullState,
                        this.bluetoothMediaState,
                        this.asiaVICSState,
                        this.asiaTPEGState,
                        this.asiaTTSState,
                        this.dragonLogoState,
                        this.selfLearningNavigationState,
                        this.bluetoothMediaOnState
                    );
                if (!this.statusbarG24.isSDSVisible() && this.renderStyle != 3) {
                    this.statusbarG24.notifyNewMainArea(partialpopupcontroller);
                } else {
                    ScreenMainArea screenmainarea = ((ScreenWidgetEVO)this.initContext.getScreen()).getMainArea();
                    this.statusbarG24.notifyNewMainArea((ContainerController)screenmainarea);
                }
            } else {
                logChannel.log(
                    10000, "StatusbarStubController#initializeWidget no statusbar G24 found (statusbarG24 == null)"
                );
            }
        }
    }

    public void predisconnecting() {
        super.predisconnecting();
        if (this.isKombiType() && this.isStatusbarValid(this.statusbarG24)) {
            this.statusbarG24.notifyNewMainArea(null);
        }
    }

    private boolean isKombiType() {
        return this.terminal != null
            && this.terminal.getFramework() != null
            && this.terminal.getFramework().getKombiType() == 4;
    }

    public void setRenderStyle(int i) {
        if (this.renderStyle != i) {
            this.renderStyle = i;
            this.setCompositesDirty(true);
            if (this.isConnected()) {
                this.updateStatusBar();
            }
        }
    }

    protected void handleFocusChanged(int i, int j, int k) {
        if (this.isStatusbarValid(this.statusbarG24)) {
            this.statusbarG24.setDrawerState(j);
        }

        super.handleFocusChanged(i, j, k);
    }

    private boolean isStatusbarValid(AbstractStatusBarController abstractstatusbarcontroller) {
        boolean flag = abstractstatusbarcontroller == null;
        if (flag) {
            logChannel.log(
                100000,
                "StatusbarStubController#isStatusbarValid statusbar %1 is not valid, it is null",
                abstractstatusbarcontroller
            );
            return false;
        } else {
            boolean flag1 = abstractstatusbarcontroller.isConnected();
            if (!flag1) {
                logChannel.log(
                    100000,
                    "StatusbarStubController#isStatusbarValid statusbar %1 is not valid, it is not connected",
                    abstractstatusbarcontroller
                );
                return false;
            } else {
                return true;
            }
        }
    }

    public IRenderer getRenderer() {
        return null;
    }

    public void processModelUpdateEvent(ModelUpdateEvent modelupdateevent) {
        super.processModelUpdateEvent(modelupdateevent);
    }

    public void setBluetoothState(int i) {
        this.bluetoothState = i;
    }

    public void setAdbImportState(int i) {
        this.adbImportState = i;
    }

    public void setAdb1ImportState(int i) {
        this.adb1ImportState = i;
    }

    public void setAdb2ImportState(int i) {
        this.adb2ImportState = i;
    }

    public void setGoogleProgressState(int i) {
        this.googleProgressState = i;
    }

    public void setGoogleTrademarkState(int i) {
        this.googleTrademarkState = i;
    }

    public void setTrafficSourceState(int i) {
        this.trafficSourceState = i;
    }

    public void setMediaImportState(int i) {
        this.mediaImportState = i;
    }

    public void setMediaMixState(int i) {
        this.mediaMixState = i;
    }

    public void setMediaRepeatState(int i) {
        this.mediaRepeatState = i;
    }

    public void setGracenoteState(int i) {
        this.gracenoteState = i;
    }

    public void setRoamingState(int i) {
        this.roamingState = i;
    }

    public void setProviderLogoState(int i) {
        this.providerLogoState = i;
    }

    public void setDragonLogoState(int i) {
        this.dragonLogoState = i;
    }

    public void setProviderNameState(int i) {
        this.providerNameState = i;
    }

    public void setWirelessChargingState(int i) {
        this.wirelessChargingState = i;
    }

    public void setProviderLogoConnectState(int i) {
        this.providerLogoConnectState = i;
    }

    public void setSmsStorageFullState(int i) {
        this.smsStorageFullState = i;
    }

    public void setBluetoothMediaState(int i) {
        this.bluetoothMediaState = i;
    }

    public void setAsiaVICSState(int i) {
        this.asiaVICSState = i;
    }

    public void setAsiaTPEGState(int i) {
        this.asiaTPEGState = i;
    }

    public void setAsiaTTSState(int i) {
        this.asiaTTSState = i;
    }

    public void setSelfLearningNavigationState(int i) {
        this.selfLearningNavigationState = i;
    }

    public void setBluetoothMediaOnState(int i) {
        this.bluetoothMediaOnState = i;
    }
}
