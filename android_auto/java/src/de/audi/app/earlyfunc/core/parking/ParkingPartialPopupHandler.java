/*
 * MU0918 stock ParkingPartialPopupHandler, rebuilt from the lsd.jar decompile (log levels restored
 * from the byte-swapped constants), plus luka-dev's PdcSmallStageGuard notifications for the SQ5
 * Android Auto PDC port:
 *   - showPopup(): arm the guard BEFORE showPartialPopup() (the compositor's SMALL_STAGE can
 *     outrun partialPopupVisible());
 *   - partialPopupVisible/Hidden/Removed, registration snapshot, unregister, deinit: keep the
 *     guard in step with the real popup.
 * The stock popup bookkeeping (visiblePPID, removeReason, cancel notification) is unchanged.
 */
package de.audi.app.earlyfunc.core.parking;

import com.sq5.aa.pdc.PdcSmallStageGuard;
import de.audi.app.car.common.app.ICarApplication;
import de.audi.app.car.common.service.CarServiceProvider;
import de.audi.atip.hmi.view.IPartialPopupListener;
import de.audi.atip.log.LogChannel;
import java.util.ArrayList;
import java.util.List;

public class ParkingPartialPopupHandler
implements IParkingPopupHandler,
IPartialPopupListener {
    private CarServiceProvider partialPopupServiceProvider;
    private final ICarApplication application;
    private final LogChannel logChannel;
    private final IParkingSystemController parkingSystemController;
    private List registeredPopupIDs;
    private volatile int visiblePPID;
    private volatile int removeReason = 1;
    private final Object mutex = AbstractParkingSystemControllerComponent.getParkingMutex();

    public ParkingPartialPopupHandler(ICarApplication iCarApplication, IParkingSystemController iParkingSystemController, LogChannel logChannel) {
        this.application = iCarApplication;
        this.parkingSystemController = iParkingSystemController;
        this.logChannel = logChannel;
        this.registeredPopupIDs = new ArrayList();
    }

    public void registerPopup(int n) {
        synchronized (this.mutex) {
            if (!this.registeredPopupIDs.contains(new Integer(n))) {
                this.registeredPopupIDs.add(new Integer(n));
                this.partialPopupServiceProvider.stopService();
                this.partialPopupServiceProvider.startService();
                this.logChannel.log(1000000, "[ParkingPartialPopupHandler#registerPopup] StartStop finished on the ServiceProvider");
            }
        }
    }

    public void unregisterPopup(int n) {
        synchronized (this.mutex) {
            this.registeredPopupIDs.remove(new Integer(n));
            pdcUnregistered(n);
            this.partialPopupServiceProvider.stopService();
            this.partialPopupServiceProvider.startService();
            this.logChannel.log(1000000, "[ParkingPartialPopupHandler#unregisterPopup] StartStop finished on the ServiceProvider");
        }
    }

    public void showPopup(int n) {
        synchronized (this.mutex) {
            if (this.registeredPopupIDs.contains(new Integer(n))) {
                if (this.visiblePPID == n) {
                    this.logChannel.log(1000000, "[ParkingPartialPopupHandler#showPopup] popupID=%1 already visible", (long)n);
                } else {
                    this.logChannel.log(1000000, "[ParkingPartialPopupHandler#showPopup] popupID=%1", (long)n);
                    pdcShowRequested(n);
                    this.application.getFrameworkAccess().getHMIService().showPartialPopup(0, n);
                }
            } else {
                this.logChannel.log(10000, "[ParkingPartialPopupHandler#showPopup] popup with ID=%1 not registered", (long)n);
            }
        }
    }

    public void removeCurrentPopup(int n) {
        synchronized (this.mutex) {
            if (this.isPopupActive()) {
                this.logChannel.log(10000000, "[ParkingPartialPopupHandler#removeCurrentPopup] cancelReason=%1", (long)n);
                this.removeReason = n;
                this.hidePartialPopup(this.visiblePPID);
            } else {
                this.logChannel.log(10000000, "[ParkingPartialPopupHandler#removeCurrentPopup] no popup active");
            }
        }
    }

    public void hidePartialPopup(int n) {
        synchronized (this.mutex) {
            this.logChannel.log(1000000, "[ParkingPartialPopupHandler#hidePartialPopup] popupID=%1", (long)n);
            this.visiblePPID = -1;
            this.application.getFrameworkAccess().getHMIService().removePartialPopup(0, n);
        }
    }

    public void initServiceProvider() {
        this.partialPopupServiceProvider = new CarServiceProvider(IPartialPopupListener.class.getName(), this, null, this.application.getBundleContext(), this.logChannel);
        this.partialPopupServiceProvider.startService();
        this.logChannel.log(1000000, "[ParkingPartialPopupHandler#initServiceProvider] Service provider initialized");
    }

    public void deinitServiceProvider() {
        try {
            PdcSmallStageGuard.parkingStopped();
        } catch (Throwable t) {
            /* stock parking must never depend on the guard */
        }
        this.partialPopupServiceProvider.stopService();
    }

    public void hidePartialPopup(int n, boolean bl) {
        synchronized (this.mutex) {
            this.logChannel.log(1000000, "[ParkingPartialPopupHandler#hidePartialPopup] popupID=%1 , cancelAfterHidden=%2", new Integer(n), Boolean.valueOf(bl));
            if (!bl) {
                this.visiblePPID = -1;
            }
            this.application.getFrameworkAccess().getHMIService().removePartialPopup(0, n);
        }
    }

    public boolean isPopupActive() {
        synchronized (this.mutex) {
            return this.visiblePPID > 0;
        }
    }

    public void partialPopupVisible(int n, int n2) {
        synchronized (this.mutex) {
            this.logChannel.log(1000000, "[ParkingPartialPopupHandler#partialPopupVisible] partialPopupID=%1, terminalID=%2", (long)n, (long)n2);
            this.visiblePPID = n;
            try {
                PdcSmallStageGuard.popupVisible(n, n2);
            } catch (Throwable t) {
                /* stock parking must never depend on the guard */
            }
        }
    }

    public void partialPopupHidden(int n, int n2) {
        this.partialPopupHidden(n, n2, false);
    }

    /* Stock body; the registration snapshot is only distinguished for the guard (luka). */
    private void partialPopupHidden(int n, int n2, boolean registration) {
        boolean bl = false;
        synchronized (this.mutex) {
            try {
                if (registration) PdcSmallStageGuard.popupRegisteredHidden(n, n2);
                else PdcSmallStageGuard.popupHidden(n, n2);
            } catch (Throwable t) {
                /* stock parking must never depend on the guard */
            }
            this.logChannel.log(1000000, "[ParkingPartialPopupHandler#partialPopupHidden] partialPopupID=%1, terminalID=%2", (long)n, (long)n2);
            if (this.isPopupActive()) {
                this.visiblePPID = -1;
                this.removeReason = 1;
                bl = true;
            }
        }
        if (bl) {
            this.parkingSystemController.notifyPartialPopupCanceled(this.removeReason);
        }
    }

    public int[] getPPIDsForCallbacks() {
        int[] nArray = new int[this.registeredPopupIDs.size()];
        for (int i2 = 0; i2 < nArray.length; ++i2) {
            nArray[i2] = ((Integer)this.registeredPopupIDs.get(i2)).intValue();
        }
        return nArray;
    }

    public void partialPopupRemoved(int n, int n2) {
        try {
            PdcSmallStageGuard.popupHidden(n, n2);
        } catch (Throwable t) {
            /* stock parking must never depend on the guard */
        }
    }

    public void partialPopupListenerRegistered(int n, int n2, boolean bl) {
        this.logChannel.log(1000000, "[ParkingPartialPopupHandler#partialPopupRegistered] partialPopupID=%1, terminalID=%2, visible=%3", (long)n, (long)n2, bl);
        if (bl) {
            this.partialPopupVisible(n, n2);
        } else {
            this.partialPopupHidden(n, n2, true);
        }
    }

    public void informAboutPPCoordinates(int n, int n2, int n3, int n4, int n5, int n6) {
    }

    private void pdcShowRequested(int n) {
        try {
            PdcSmallStageGuard.showRequested(n, this.application.getFrameworkAccess().getHMIService());
        } catch (Throwable t) {
            /* stock parking must never depend on the guard */
        }
    }

    private static void pdcUnregistered(int n) {
        try {
            PdcSmallStageGuard.popupUnregistered(n);
        } catch (Throwable t) {
            /* stock parking must never depend on the guard */
        }
    }
}
