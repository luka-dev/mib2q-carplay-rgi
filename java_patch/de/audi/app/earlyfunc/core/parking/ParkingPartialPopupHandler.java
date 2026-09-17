package de.audi.app.earlyfunc.core.parking;

import com.luka.carplay.pdc.PdcSmallStageGuard;
import de.audi.app.car.common.app.ICarApplication;
import de.audi.app.car.common.service.CarServiceProvider;
import de.audi.atip.hmi.view.IPartialPopupListener;
import de.audi.atip.log.LogChannel;
import java.util.ArrayList;
import java.util.List;

/**
 * Stock MU1316 handler with one narrow hook for pure OPS popup 2100008.
 * The guard is raised before showPartialPopup() so the compositor's
 * SMALL_STAGE callback cannot outrun partialPopupVisible().
 */
public class ParkingPartialPopupHandler implements IParkingPopupHandler, IPartialPopupListener {
    private CarServiceProvider partialPopupServiceProvider;
    private final ICarApplication application;
    private final LogChannel logChannel;
    private final IParkingSystemController parkingSystemController;
    private List registeredPopupIDs;
    private volatile int visiblePPID;
    private volatile int removeReason = 1;
    private final Object mutex = AbstractParkingSystemControllerComponent.getParkingMutex();

    public ParkingPartialPopupHandler(
        ICarApplication application,
        IParkingSystemController parkingSystemController,
        LogChannel logChannel
    ) {
        this.application = application;
        this.parkingSystemController = parkingSystemController;
        this.logChannel = logChannel;
        this.registeredPopupIDs = new ArrayList();
    }

    public void registerPopup(int popupId) {
        synchronized (this.mutex) {
            if (!this.registeredPopupIDs.contains(new Integer(popupId))) {
                this.registeredPopupIDs.add(new Integer(popupId));
                this.partialPopupServiceProvider.stopService();
                this.partialPopupServiceProvider.startService();
                this.logChannel.log(
                    1000000,
                    "[ParkingPartialPopupHandler#registerPopup] StartStop finished on the ServiceProvider"
                );
            }
        }
    }

    public void unregisterPopup(int popupId) {
        synchronized (this.mutex) {
            this.registeredPopupIDs.remove(new Integer(popupId));
            PdcSmallStageGuard.popupUnregistered(popupId);
            this.partialPopupServiceProvider.stopService();
            this.partialPopupServiceProvider.startService();
            this.logChannel.log(
                1000000,
                "[ParkingPartialPopupHandler#unregisterPopup] StartStop finished on the ServiceProvider"
            );
        }
    }

    public void showPopup(int popupId) {
        synchronized (this.mutex) {
            if (this.registeredPopupIDs.contains(new Integer(popupId))) {
                if (this.visiblePPID == popupId) {
                    this.logChannel.log(
                        1000000,
                        "[ParkingPartialPopupHandler#showPopup] popupID=%1 already visible",
                        popupId
                    );
                } else {
                    this.logChannel.log(
                        1000000,
                        "[ParkingPartialPopupHandler#showPopup] popupID=%1",
                        popupId
                    );
                    PdcSmallStageGuard.showRequested(
                        popupId,
                        this.application.getFrameworkAccess().getHMIService()
                    );
                    this.application.getFrameworkAccess().getHMIService().showPartialPopup(0, popupId);
                }
            } else {
                this.logChannel.log(
                    10000,
                    "[ParkingPartialPopupHandler#showPopup] popup with ID=%1 not registered",
                    popupId
                );
            }
        }
    }

    public void removeCurrentPopup(int cancelReason) {
        synchronized (this.mutex) {
            if (this.isPopupActive()) {
                this.logChannel.log(
                    10000000,
                    "[ParkingPartialPopupHandler#removeCurrentPopup] cancelReason=%1",
                    cancelReason
                );
                this.removeReason = cancelReason;
                this.hidePartialPopup(this.visiblePPID);
            } else {
                this.logChannel.log(
                    10000000,
                    "[ParkingPartialPopupHandler#removeCurrentPopup] no popup active"
                );
            }
        }
    }

    public void hidePartialPopup(int popupId) {
        synchronized (this.mutex) {
            this.logChannel.log(
                1000000,
                "[ParkingPartialPopupHandler#hidePartialPopup] popupID=%1",
                popupId
            );
            this.visiblePPID = -1;
            this.application.getFrameworkAccess().getHMIService().removePartialPopup(0, popupId);
        }
    }

    public void initServiceProvider() {
        this.partialPopupServiceProvider = new CarServiceProvider(
            IPartialPopupListener.class.getName(),
            this,
            null,
            this.application.getBundleContext(),
            this.logChannel
        );
        this.partialPopupServiceProvider.startService();
        this.logChannel.log(
            1000000,
            "[ParkingPartialPopupHandler#initServiceProvider] Service provider initialized"
        );
    }

    public void deinitServiceProvider() {
        PdcSmallStageGuard.parkingStopped();
        this.partialPopupServiceProvider.stopService();
    }

    public void hidePartialPopup(int popupId, boolean cancelAfterHidden) {
        synchronized (this.mutex) {
            this.logChannel.log(
                1000000,
                "[ParkingPartialPopupHandler#hidePartialPopup] popupID=%1 , cancelAfterHidden=%2",
                new Integer(popupId),
                new Boolean(cancelAfterHidden)
            );
            if (!cancelAfterHidden) {
                this.visiblePPID = -1;
            }

            this.application.getFrameworkAccess().getHMIService().removePartialPopup(0, popupId);
        }
    }

    public boolean isPopupActive() {
        synchronized (this.mutex) {
            return this.visiblePPID > 0;
        }
    }

    public void partialPopupVisible(int popupId, int terminalId) {
        if (terminalId != PdcSmallStageGuard.MAIN_TERMINAL) return;
        synchronized (this.mutex) {
            this.logChannel.log(
                1000000,
                "[ParkingPartialPopupHandler#partialPopupVisible] partialPopupID=%1, terminalID=%2",
                popupId,
                terminalId
            );
            this.visiblePPID = popupId;
            PdcSmallStageGuard.popupVisible(popupId, terminalId);
        }
    }

    public void partialPopupHidden(int popupId, int terminalId) {
        partialPopupHidden(popupId, terminalId, false);
    }

    private void partialPopupHidden(int popupId, int terminalId, boolean registration) {
        if (terminalId != PdcSmallStageGuard.MAIN_TERMINAL) return;
        boolean notifyCanceled = false;
        synchronized (this.mutex) {
            if (registration) PdcSmallStageGuard.popupRegisteredHidden(popupId, terminalId);
            else PdcSmallStageGuard.popupHidden(popupId, terminalId);
            this.logChannel.log(
                1000000,
                "[ParkingPartialPopupHandler#partialPopupHidden] partialPopupID=%1, terminalID=%2",
                popupId,
                terminalId
            );
            /* A registration snapshot or late hide for a different popup must
             * not cancel the currently visible parking popup. HMI reports the
             * popup ID and terminal separately; both must identify this one. */
            if (this.visiblePPID == popupId && this.isPopupActive()) {
                this.visiblePPID = -1;
                this.removeReason = 1;
                notifyCanceled = true;
            }
        }

        if (notifyCanceled) {
            this.parkingSystemController.notifyPartialPopupCanceled(this.removeReason);
        }
    }

    public int[] getPPIDsForCallbacks() {
        int[] popupIds = new int[this.registeredPopupIDs.size()];
        for (int i = 0; i < popupIds.length; i++) {
            popupIds[i] = ((Integer)this.registeredPopupIDs.get(i)).intValue();
        }
        return popupIds;
    }

    public void partialPopupRemoved(int popupId, int terminalId) {
        PdcSmallStageGuard.popupHidden(popupId, terminalId);
    }

    public void partialPopupListenerRegistered(int popupId, int terminalId, boolean isVisible) {
        this.logChannel.log(
            1000000,
            "[ParkingPartialPopupHandler#partialPopupRegistered] partialPopupID=%1, terminalID=%2, visible=%3",
            popupId,
            terminalId,
            isVisible
        );
        if (isVisible) {
            this.partialPopupVisible(popupId, terminalId);
        } else {
            this.partialPopupHidden(popupId, terminalId, true);
        }
    }

    public void informAboutPPCoordinates(
        int popupId,
        int terminalId,
        int x,
        int y,
        int width,
        int height
    ) {
    }
}
