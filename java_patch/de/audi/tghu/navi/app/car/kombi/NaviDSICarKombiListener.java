package de.audi.tghu.navi.app.car.kombi;

import com.luka.carplay.core.VcLayoutState;

import de.audi.atip.log.LogChannel;
import java.util.ArrayList;
import org.dsi.ifc.carkombi.BCViewOptions;
import org.dsi.ifc.carkombi.DCDisplayPresetsListRecord;
import org.dsi.ifc.carkombi.DCDisplayViewConfiguration;
import org.dsi.ifc.carkombi.DSICarKombi;
import org.dsi.ifc.global.CarArrayListUpdateInfo;

/*
 * Stock classpath override with one narrow addition: retain the existing
 * BCViewOptions observer behaviour verbatim, and subscribe the already-live
 * DSICarKombi listener to the four read-only display-preset attributes needed
 * to distinguish Classic from Sport.  No second DSIActivator is created.
 */
public class NaviDSICarKombiListener extends AbstractNaviDSICarKombiListner
        implements ICarKombiEventsProvider {
    private final LogChannel logChannel;
    private final ArrayList observersList;
    private BCViewOptions lastBCViewOptions = null;

    public NaviDSICarKombiListener(LogChannel logchannel) {
        this.logChannel = logchannel;
        this.observersList = new ArrayList();
    }

    public void updateBCViewOptions(BCViewOptions bcviewoptions, int i) {
        this.logChannel.log(1000000,
            "NaviDSICarKombiListener#updateBCViewOptions( %1, %2 )",
            bcviewoptions, i);
        if (i != 1) {
            this.logChannel.log(1000000,
                "NaviDSICarKombiListener#updateBCViewOptions() - update is invalid");
        } else if (bcviewoptions == null) {
            this.logChannel.log(1000000,
                "NaviDSICarKombiListener#updateBCViewOptions() - options are null");
        } else {
            synchronized (this.observersList) {
                java.util.Iterator it = this.observersList.iterator();
                while (it.hasNext()) {
                    ICarKombiObserver observer = (ICarKombiObserver)it.next();
                    try {
                        observer.updateBCViewOptions(bcviewoptions);
                    } catch (Exception exception) {
                        this.logChannel.log(10000,
                            "NaviDSICarKombiListener#updateBCViewOptions() - observer failed",
                            exception);
                    }
                }
                this.lastBCViewOptions = bcviewoptions;
            }
        }
    }

    public int[] getAutoNotifications() {
        return new int[] {
            DSICarKombi.ATTR_BCVIEWOPTIONS,
            DSICarKombi.ATTR_DCDISPLAYPRESETSLISTTOTALNUMBEROFELEMENTS,
            DSICarKombi.ATTR_DCDISPLAYPRESETSLISTUPDATEINFO,
            DSICarKombi.ATTR_DCACTIVEDISPLAYPRESET,
            DSICarKombi.ATTR_DCDISPLAYVIEWCONFIGURATION
        };
    }

    public void updateDCDisplayPresetsListTotalNumberOfElements(int count,
                                                                 int validFlag) {
        VcLayoutState.onPresetCount(count, validFlag);
    }

    public void responseDCDisplayPresetsList(CarArrayListUpdateInfo info,
                                              DCDisplayPresetsListRecord[] records) {
        VcLayoutState.onPresetRecords("response", records, 1);
    }

    public void updateDCDisplayPresetsListUpdateInfo(CarArrayListUpdateInfo info,
                                                      DCDisplayPresetsListRecord[] records,
                                                      int validFlag) {
        VcLayoutState.onPresetRecords("update", records, validFlag);
    }

    public void updateDCActiveDisplayPreset(int preset, int validFlag) {
        VcLayoutState.onActivePreset(preset, validFlag);
    }

    public void updateDCDisplayViewConfiguration(DCDisplayViewConfiguration config,
                                                  int validFlag) {
        VcLayoutState.onDisplayViewConfiguration(config, validFlag);
    }

    public void registerListener(ICarKombiObserver observer) {
        synchronized (this.observersList) {
            if (observer == null) throw new IllegalArgumentException();
            this.observersList.add(observer);
            if (this.lastBCViewOptions != null) {
                try {
                    observer.updateBCViewOptions(this.lastBCViewOptions);
                } catch (Exception exception) {
                    this.logChannel.log(10000,
                        "NaviDSICarKombiListener#registerListener() - observer failed",
                        exception);
                }
            }
        }
    }

    public void unregisterListener(ICarKombiObserver observer) {
    }
}
