/*
 * Stock MU1316 PLA message routing with one projection-side OPS policy:
 * the separate bottom warning banner is suppressed while pure OPS is shown
 * beside CarPlay. Full-screen camera and active PLA modes remain stock.
 *
 * Java 1.2 compatible.
 */
package de.audi.app.earlyfunc.core.parking.pla;

import com.luka.carplay.pdc.PdcSmallStageGuard;
import de.audi.app.car.common.app.ICarApplication;
import de.audi.app.earlyfunc.core.parking.IParkingSystemController;
import de.audi.atip.log.LogChannel;
import de.audi.atip.model.ICoreEarlyAppsModelBank;

public class PLAMessageHandler implements IPLAMessageHandler {
    private static final int[] SINGLE_LINE_MESSAGES = new int[]{18, 33, 34, 35};
    private final ICarApplication application;
    private final AbstractParkingSystemPLAComponent plaComponent;
    private final IParkingSystemController controller;
    private final LogChannel logChannel;
    private final int defaultMessagePartialPopupID;
    private final int splitscreenCenteredMessagePartialPopupID;
    private final int singleLineMessagePartialPopupID;
    private final int opsStandaloneLeftMessagePartialPopupID;
    private final int opsStandaloneRightMessagePartialPopupID;
    private final int startParkOutMessagePartialPopupID;
    private volatile int visiblePartialPopupId = -1;
    private int currentMessageID = 0;
    private int plaOpsStandaloneState = 0;

    public PLAMessageHandler(
        ICarApplication icarapplication,
        IParkingSystemController iparkingsystemcontroller,
        AbstractParkingSystemPLAComponent abstractparkingsystemplacomponent,
        LogChannel logchannel,
        int i,
        int j,
        int k,
        int l,
        int i1,
        int j1
    ) {
        this.application = icarapplication;
        this.controller = iparkingsystemcontroller;
        this.plaComponent = abstractparkingsystemplacomponent;
        this.logChannel = logchannel;
        this.defaultMessagePartialPopupID = i;
        this.splitscreenCenteredMessagePartialPopupID = j;
        this.singleLineMessagePartialPopupID = k;
        this.opsStandaloneLeftMessagePartialPopupID = l;
        this.opsStandaloneRightMessagePartialPopupID = i1;
        this.startParkOutMessagePartialPopupID = j1;
    }

    public void init() {
    }

    public void deinit() {
    }

    public void showMessage(int i) {
        if (this.isMessageSupported(i)) {
            this.logChannel.log(1000000, "[PLAMessageHandler#showMessage] messageID='%1'", i);
            this.currentMessageID = i;

            if (PdcSmallStageGuard.shouldSuppressParkingMessagePopup()) {
                this.hideVisiblePopup();
                return;
            }

            int j = this.getPartialPopupID(i);
            if (-1 != this.visiblePartialPopupId && (i == 0 || this.visiblePartialPopupId != j)) {
                this.hideVisiblePopup();
            }

            if (i != 0) {
                this.application
                    .getFrameworkAccess()
                    .getHmiServiceApp()
                    .getChoiceModel(ICoreEarlyAppsModelBank.PARKING_PLA_MESSAGE_ID_CHOICE)
                    .setValue(i);
            }

            if (i > 0) {
                this.visiblePartialPopupId = j;
                this.application.getFrameworkAccess().getHmiServiceApp().showPartialPopup(0, j);
                /* Close the small race between the initial policy check and
                 * an OPS activation on the parking dispatcher. */
                if (PdcSmallStageGuard.shouldSuppressParkingMessagePopup()) {
                    this.hideVisiblePopup();
                }
            }
        } else {
            this.logChannel.log(1000000, "[PLAMessageHandler#showMessage] messageID='%1' rejected - not supported", i);
            this.hideVisiblePopup();
        }
    }

    public void updateMessage() {
        this.logChannel.log(1000000, "[PLAMessageHandler#updateMessage] called.");
        if (0 != this.currentMessageID) {
            this.showMessage(this.currentMessageID);
        }
    }

    public void setPlaOpsStandaloneState(int i) {
        this.plaOpsStandaloneState = i;
    }

    private void hideVisiblePopup() {
        int popupId = this.visiblePartialPopupId;
        if (popupId == -1) return;
        /* Publish hidden before the HMI callback can synchronously re-enter. */
        this.visiblePartialPopupId = -1;
        this.application
            .getFrameworkAccess()
            .getHmiServiceApp()
            .removePartialPopup(0, popupId);
    }

    private int getPartialPopupID(int i) {
        boolean flag = this.controller.getOPSViewModeHandler().isOpsActive();
        int j;
        if (16 == i) {
            j = this.startParkOutMessagePartialPopupID;
        } else {
            switch (this.plaComponent.getCurrentPlaMode()) {
                case 0:
                default:
                    j = this.isSingleLineMessage(i)
                        ? this.singleLineMessagePartialPopupID
                        : (flag ? this.splitscreenCenteredMessagePartialPopupID : this.defaultMessagePartialPopupID);
                    break;
                case 1:
                case 2:
                case 3:
                case 6:
                    j = this.isSingleLineMessage(i)
                        ? this.singleLineMessagePartialPopupID
                        : this.defaultMessagePartialPopupID;
                    break;
                case 4:
                case 5:
                    if (1 == this.plaOpsStandaloneState) {
                        j = this.isSingleLineMessage(i)
                            ? this.singleLineMessagePartialPopupID
                            : this.opsStandaloneLeftMessagePartialPopupID;
                    } else if (2 == this.plaOpsStandaloneState) {
                        j = this.isSingleLineMessage(i)
                            ? this.singleLineMessagePartialPopupID
                            : this.opsStandaloneRightMessagePartialPopupID;
                    } else {
                        j = this.isSingleLineMessage(i)
                            ? this.singleLineMessagePartialPopupID
                            : (flag ? this.splitscreenCenteredMessagePartialPopupID : this.defaultMessagePartialPopupID);
                    }
            }
        }

        return j;
    }

    private boolean isSingleLineMessage(int i) {
        for (int j = 0; j < SINGLE_LINE_MESSAGES.length; j++) {
            if (i == SINGLE_LINE_MESSAGES[j]) return true;
        }
        return false;
    }

    private boolean isMessageSupported(int i) {
        return i <= 40;
    }
}
