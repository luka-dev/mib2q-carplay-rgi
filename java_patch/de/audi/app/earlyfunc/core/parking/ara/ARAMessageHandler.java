/*
 * Stock MU1316 ARA message routing with the same projection-side OPS policy
 * as PLAMessageHandler. A stale/queued trailer-assist instruction must not
 * occupy the lower parking-message slot beside CarPlay.
 *
 * Java 1.2 compatible.
 */
package de.audi.app.earlyfunc.core.parking.ara;

import com.luka.carplay.pdc.PdcSmallStageGuard;
import de.audi.app.car.common.app.ICarApplication;
import de.audi.app.earlyfunc.core.parking.IParkingSystemController;
import de.audi.atip.log.LogChannel;
import de.audi.atip.model.ICoreEarlyAppsModelBank;

public class ARAMessageHandler implements IARAMessageHandler {
    private final ICarApplication application;
    private final IParkingSystemController controller;
    private final LogChannel logChannel;
    private final int defaultMessagePartialPopupID;
    private final int splitscreenCenteredMessagePartialPopupID;
    private int visiblePartialPopupId = -1;
    private int currentMessageID = 0;

    public ARAMessageHandler(
        ICarApplication application,
        IParkingSystemController controller,
        LogChannel logChannel,
        int defaultMessagePartialPopupID,
        int splitscreenCenteredMessagePartialPopupID
    ) {
        this.application = application;
        this.controller = controller;
        this.logChannel = logChannel;
        this.defaultMessagePartialPopupID = defaultMessagePartialPopupID;
        this.splitscreenCenteredMessagePartialPopupID = splitscreenCenteredMessagePartialPopupID;
    }

    public void init() {
    }

    public void deinit() {
    }

    public void showMessage(int messageID) {
        this.logChannel.log(1000000, "[ARAMessageHandler#showMessage] id='%1'", messageID);
        this.currentMessageID = messageID;

        if (PdcSmallStageGuard.shouldSuppressParkingMessagePopup()) {
            this.hideVisiblePopup();
            return;
        }

        int popupID = this.getPartialPopupID();
        if (this.visiblePartialPopupId != -1
            && (messageID == 0 || this.visiblePartialPopupId != popupID)) {
            this.hideVisiblePopup();
        }

        if (messageID != 0) {
            this.application
                .getFrameworkAccess()
                .getHmiServiceApp()
                .getChoiceModel(ICoreEarlyAppsModelBank.PARKING_ARA_MESSAGE_ID_CHOICE)
                .setValue(messageID);
        }

        if (messageID > 0) {
            this.visiblePartialPopupId = popupID;
            this.application.getFrameworkAccess().getHmiServiceApp().showPartialPopup(0, popupID);
            if (PdcSmallStageGuard.shouldSuppressParkingMessagePopup()) {
                this.hideVisiblePopup();
            }
        }
    }

    public void updateMessage() {
        this.logChannel.log(1000000, "[ARAMessageHandler#updateMessage] called.");
        if (this.currentMessageID != 0) this.showMessage(this.currentMessageID);
    }

    private int getPartialPopupID() {
        return this.controller.getOPSViewModeHandler().isOpsActive()
            ? this.splitscreenCenteredMessagePartialPopupID
            : this.defaultMessagePartialPopupID;
    }

    private void hideVisiblePopup() {
        int popupID = this.visiblePartialPopupId;
        if (popupID == -1) return;
        this.visiblePartialPopupId = -1;
        this.application
            .getFrameworkAccess()
            .getHmiServiceApp()
            .removePartialPopup(0, popupID);
    }
}
