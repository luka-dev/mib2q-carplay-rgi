package de.audi.app.earlyfunc.evo.parking;

import com.luka.carplay.pdc.PdcSmallStageGuard;
import de.audi.app.car.common.app.ICarApplication;
import de.audi.app.earlyfunc.core.parking.AbstractParkingSystemControllerComponent;
import de.audi.app.earlyfunc.core.parking.IParkingFocusPropertyConfig;
import de.audi.app.earlyfunc.core.parking.IParkingSystem;
import de.audi.app.earlyfunc.core.parking.ParkingFocusPropertyCollection;
import de.audi.atip.hmi.IDrawerCategoryEarlyApps;
import de.audi.atip.hmi.model.property.PropertyModelApp;
import de.audi.atip.model.IEvoEarlyAppsModelBank;
import org.dsi.ifc.carparkingsystem.ParkingSystemViewOptions;
import org.dsi.ifc.carparkingsystem.DisplayContent;
import java.util.List;

/** MU1316 controller with parking intent published before any 108 notification. */
public class ParkingSystemControllerComponentEvo
    extends AbstractParkingSystemControllerComponent
    implements IParkingFocusPropertyConfig {
    IParkingFocusPropertyConfig propertyConfigChain = this;
    private ParkingFocusPropertyCollection focusProperties = new ParkingFocusPropertyCollection();

    public ParkingSystemControllerComponentEvo(ICarApplication icarapplication) {
        super(icarapplication);
    }

    protected void deactivateCurrentlyVisibleParkingSystems(DisplayContent content, List nextSystems) {
        /* activateParkingSystem calls this BEFORE activating even the first
         * component (VPS is first). currentDisplayContent is still the OLD
         * content here. Classify the supplied target, never the visible popup. */
        PdcSmallStageGuard.parkingContentChanging(
            content, nextSystems, this.getApplication().getFrameworkAccess().getHMIService()
        );
        super.deactivateCurrentlyVisibleParkingSystems(content, nextSystems);
    }

    public void notifyParkingSystemActive(IParkingSystem system, boolean active) {
        /* VPS/PLA can also notify independently of a content transition.
         * Revoke OPS permission before their stock notification sends 108. */
        if (active && (system == null || system.getParkingSystemID() != IParkingSystem.PARKING_SYSTEM_OPS)) {
            PdcSmallStageGuard.parkingStopped();
        }
        super.notifyParkingSystemActive(system, active);
    }

    protected void updateMenuEntryVisibility(ParkingSystemViewOptions parkingsystemviewoptions) {
        this.getApplication()
            .getMenuEntryRegistry()
            .updateMenuEntryVisibility(
                1006, this.getMenuEntryVisibilityState(parkingsystemviewoptions.pdcPLASystemState)
            );
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

    public synchronized void registerParkingSystemComponent(IParkingSystem iparkingsystem) {
        super.registerParkingSystemComponent(iparkingsystem);
        this.propertyConfigChain = iparkingsystem.createFocusPropertyDecorator(this.propertyConfigChain);
    }

    public synchronized void reconfigParkingOptionDrawer() {
        PropertyModelApp propertymodelapp = this.getPropertyModel(
            IEvoEarlyAppsModelBank.PARKING_SYSTEM_FOCUS_OPT_DRAWER_PROPERTY
        );
        this.getLogChannel()
            .log(
                10000000,
                "[ParkingSystemControllerComponentEvo#reconfigParkingOptionDrawer] collects focus properties and sets PropertyModel for ParkingSystems: modelID='%1' , category='CATEGORIE_POPUPS_FAHRZEUG_CAR_EINPARKHILFE(%2)'",
                propertymodelapp.getID(),
                1459086142L
            );
        this.propertyConfigChain.configureFocusProperties(new ParkingFocusPropertyCollection());
        propertymodelapp.setProperties(
            IDrawerCategoryEarlyApps.CATEGORIE_POPUPS_FAHRZEUG_CAR_EINPARKHILFE,
            this.focusProperties.getFocusPropertyArray()
        );
    }

    public void configureFocusProperties(ParkingFocusPropertyCollection parkingfocuspropertycollection) {
        if (parkingfocuspropertycollection != null) {
            this.focusProperties = parkingfocuspropertycollection;
        }
    }

    public void init() {
        super.init();
        this.getOPSViewModeHandler().init(2100001);
    }

    public void deinit() {
        PdcSmallStageGuard.parkingStopped();
        this.getOPSViewModeHandler().deinit();
        super.deinit();
    }
}
