/*
 * MU0918 stock ParkingSystemControllerComponentEvo, rebuilt from the lsd.jar decompile (constants
 * restored from lsd.jxe), plus luka-dev's two parking-intent hooks for the SQ5 Android Auto PDC port:
 *   - deactivateCurrentlyVisibleParkingSystems(): publish the NEW DisplayContent + component list
 *     to PdcSmallStageGuard before stock activates/deactivates any component;
 *   - notifyParkingSystemActive(): a non-OPS system (camera/VPS, ARA, PLA) becoming active revokes
 *     the guard before its stock notification;
 *   - deinit(): release the guard.
 * Everything else is the MU0918 original.
 */
package de.audi.app.earlyfunc.evo.parking;

import com.sq5.aa.pdc.PdcSmallStageGuard;
import de.audi.app.car.common.app.ICarApplication;
import de.audi.app.earlyfunc.core.parking.AbstractParkingSystemControllerComponent;
import de.audi.app.earlyfunc.core.parking.IParkingFocusPropertyConfig;
import de.audi.app.earlyfunc.core.parking.IParkingSystem;
import de.audi.app.earlyfunc.core.parking.ParkingFocusPropertyCollection;
import de.audi.atip.hmi.model.property.PropertyModelApp;
import java.util.List;
import org.dsi.ifc.carparkingsystem.DisplayContent;
import org.dsi.ifc.carparkingsystem.ParkingSystemViewOptions;

public class ParkingSystemControllerComponentEvo
extends AbstractParkingSystemControllerComponent
implements IParkingFocusPropertyConfig {
    IParkingFocusPropertyConfig propertyConfigChain = this;
    private ParkingFocusPropertyCollection focusProperties = new ParkingFocusPropertyCollection();

    public ParkingSystemControllerComponentEvo(ICarApplication iCarApplication) {
        super(iCarApplication);
    }

    /* SQ5 PDC: activateParkingSystem() calls this BEFORE activating even the first component;
     * currentDisplayContent is still the OLD content here, so classify the supplied target. */
    protected void deactivateCurrentlyVisibleParkingSystems(DisplayContent displayContent, List list) {
        try {
            PdcSmallStageGuard.parkingContentChanging(displayContent, list,
                this.getApplication().getFrameworkAccess().getHMIService());
        } catch (Throwable t) {
            /* stock parking must never depend on the guard */
        }
        super.deactivateCurrentlyVisibleParkingSystems(displayContent, list);
    }

    /* SQ5 PDC: VPS/ARA/PLA can also notify independently of a content transition. */
    public void notifyParkingSystemActive(IParkingSystem iParkingSystem, boolean bl) {
        if (bl && (iParkingSystem == null || iParkingSystem.getParkingSystemID() != 2 /* OPS */)) {
            try {
                PdcSmallStageGuard.parkingStopped();
            } catch (Throwable t) {
                /* stock parking must never depend on the guard */
            }
        }
        super.notifyParkingSystemActive(iParkingSystem, bl);
    }

    protected void updateMenuEntryVisibility(ParkingSystemViewOptions parkingSystemViewOptions) {
        this.getApplication().getMenuEntryRegistry().updateMenuEntryVisibility(1006, this.getMenuEntryVisibilityState(parkingSystemViewOptions.pdcPLASystemState));
    }

    protected void initModels() {
        this.reconfigParkingOptionDrawer();
    }

    protected void deinitModels() {
    }

    protected void initVisibility() {
        this.getApplication().getMenuEntryRegistry().registerMenuEntry(1006, (short)2);
    }

    protected void deinitVisibility() {
        this.getApplication().getMenuEntryRegistry().deregisterMenuEntry(1006);
    }

    public int getID() {
        return 1;
    }

    public int getStandbyPopupID() {
        return 6;
    }

    public synchronized void registerParkingSystemComponent(IParkingSystem iParkingSystem) {
        super.registerParkingSystemComponent(iParkingSystem);
        this.propertyConfigChain = iParkingSystem.createFocusPropertyDecorator(this.propertyConfigChain);
    }

    public synchronized void reconfigParkingOptionDrawer() {
        /* 2100336 = IEvoEarlyAppsModelBank.PARKING_SYSTEM_FOCUS_OPT_DRAWER_PROPERTY,
         * 1459086142 = IDrawerCategoryEarlyApps.CATEGORIE_POPUPS_FAHRZEUG_CAR_EINPARKHILFE */
        PropertyModelApp propertyModelApp = this.getPropertyModel(2100336);
        this.getLogChannel().log(10000000, "[ParkingSystemControllerComponentEvo#reconfigParkingOptionDrawer] collects focus properties and sets PropertyModel for ParkingSystems: modelID='%1' , category='CATEGORIE_POPUPS_FAHRZEUG_CAR_EINPARKHILFE(%2)'", (long)propertyModelApp.getID(), 1459086142L);
        this.propertyConfigChain.configureFocusProperties(new ParkingFocusPropertyCollection());
        propertyModelApp.setProperties(1459086142, this.focusProperties.getFocusPropertyArray());
    }

    public void configureFocusProperties(ParkingFocusPropertyCollection parkingFocusPropertyCollection) {
        if (parkingFocusPropertyCollection != null) {
            this.focusProperties = parkingFocusPropertyCollection;
        }
    }

    public void init() {
        super.init();
        this.getOPSViewModeHandler().init(2100001);
    }

    public void deinit() {
        try {
            PdcSmallStageGuard.parkingStopped();
        } catch (Throwable t) {
            /* stock parking must never depend on the guard */
        }
        this.getOPSViewModeHandler().deinit();
        super.deinit();
    }
}
