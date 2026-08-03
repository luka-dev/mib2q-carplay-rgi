package de.audi.tghu.navi.app.cluster;

import de.audi.atip.hmi.modelaccess.ChoiceModelApp;
import de.audi.atip.interapp.combi.ddp2.CombiService;
import de.audi.atip.log.LogChannel;
import de.audi.tghu.navi.app.NavigationEnv;
import de.audi.tghu.navi.app.util.Util;
import de.esolutions.fw.util.commons.Buffer;
import com.luka.carplay.probe.ClusterProbe;

public class ClusterViewMode {
    public static final int VIEWMODE_COMPASS = 0;
    public static final int VIEWMODE_RGI = 1;
    public static final int VIEWMODE_KDK = 2;
    public static final int VIEWMODE_MAP = 3;
    private final NavigationEnv env;
    private LogChannel logChannel;
    private ClusterService clusterService;
    private CombiService combiService;
    private int favoredViewMode;
    private boolean favoredViewModeReceived = false;
    private boolean rgiValid;
    private boolean gfxAvailable;
    private boolean mapReady;
    private boolean komoViewEnabled;
    private boolean komoViewVisible;
    private int dataRate;
    private ChoiceModelApp viewMode;

    public ClusterViewMode(NavigationEnv navigationenv, ClusterService clusterservice) {
        this.env = navigationenv;
        this.logChannel = navigationenv.getLogChannel();
        this.clusterService = clusterservice;
        this.viewMode = navigationenv.getChoiceModel(67);
        synchronized (this) {
            this.favoredViewMode = 1;
            this.rgiValid = false;
            this.gfxAvailable = false;
            this.komoViewEnabled = Util.isClusterMapMOSTAlwaysOn();
            this.komoViewVisible = false;
            this.dataRate = 0;
            this.mapReady = false;
            this.viewMode.setValue(0);
        }
    }

    public void setCombiService(CombiService combiservice) {
        this.logChannel.log(10000000, "ClusterViewMode#setCombiService( %1 )", combiservice);
        this.combiService = combiservice;
        this.refreshRGState();
    }

    public void refreshRGState() {
        synchronized (this) {
            if (this.combiService != null) {
                this.logChannel.log(10000000, "ClusterViewMode#refreshRGState()");
                boolean flag = this.env.getContainer().isRgActive();
                this.combiService.updateRGState(flag ? 6 : 1);
            } else {
                this.logChannel.log(100000, "ClusterViewMode#refreshRGState() - combiService not available!");
            }

            this.refreshViewMode();
        }
    }

    public void setFavoredViewMode(int i) {
        synchronized (this) {
            this.favoredViewModeReceived = true;
            if (this.favoredViewMode != i) {
                this.logChannel.log(10000000, "ClusterViewMode#setFavoredViewMode( %1 )", viewModeToString(i));
                this.favoredViewMode = i;
            }

            this.refreshViewMode();
        }
    }

    public void setRGIValid(boolean flag) {
        synchronized (this) {
            if (this.rgiValid != flag) {
                this.rgiValid = flag;
                this.refreshViewMode();
            }
        }
    }

    public void setGFXAvailable(boolean flag) {
        synchronized (this) {
            if (this.gfxAvailable != flag) {
                this.gfxAvailable = flag;
                this.refreshViewMode();
                if (flag) {
                    this.clusterService.setKOMODataRate(this.dataRate);
                } else {
                    this.clusterService.setKOMODataRate(0);
                }
            }
        }
    }

    public void setKOMOViewEnabled(boolean flag) {
        synchronized (this) {
            if (this.komoViewEnabled != flag) {
                this.komoViewEnabled = flag;
                this.refreshViewMode();
            }
        }
    }

    public void setKOMOViewVisible(boolean flag) {
        synchronized (this) {
            if (this.komoViewVisible != flag) {
                this.komoViewVisible = flag;
                this.refreshViewMode();
            }
        }
    }

    public void setDataRate(int i) {
        synchronized (this) {
            if (this.dataRate != i) {
                this.dataRate = i;
                this.refreshMapVisibility();
            }
        }
    }

    public void setKombiMapReady(boolean flag) {
        synchronized (this) {
            if (this.mapReady != flag) {
                this.mapReady = flag;
                this.refreshViewMode();
            }
        }
    }

    private void refreshViewMode() {
        if (ClusterProbe.on()) ClusterProbe.log("refreshViewMode: kombiType=" + this.env.getFramework().getKombiType()
            + " FPK=" + Util.isClusterMapFPK(this.env.getFramework()) + " MMI=" + Util.isClusterMMI(this.env.getFramework())
            + " MOST=" + Util.isClusterMapMOST(this.env.getFramework()) + " cur=" + this.viewMode.getValue()
            + " fav=" + this.favoredViewMode + " gfx=" + this.gfxAvailable + " mapReady=" + this.mapReady
            + " rgiValid=" + this.rgiValid);
        if (!Util.isClusterMapFPK(this.env.getFramework()) && !Util.isClusterMMI(this.env.getFramework())) {
            int i = this.viewMode.getValue();
            this.logChannel
                .log(
                    10000000,
                    "ClusterViewMode#refreshViewMode() - current: %1, favored: %2",
                    viewModeToString(i),
                    viewModeToString(this.favoredViewMode)
                );
            if (this.favoredViewMode == 0) {
                this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - switch to COMPASS");
                this.viewMode.setValue(0);
            } else {
                switch (i) {
                    case 1:
                        if (this.favoredViewMode == 1) {
                            if (!this.rgiValid) {
                                this.logChannel
                                    .log(100000, "ClusterViewMode#refreshViewMode() - RGI not longer valid!");
                                this.viewMode.setValue(0);
                            } else {
                                this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - stay in RGI");
                            }
                        } else if (this.favoredViewMode == 2) {
                            if (this.isKDKReady()) {
                                this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - switch to KDK");
                                this.viewMode.setValue(2);
                            } else if (!this.rgiValid) {
                                this.logChannel
                                    .log(100000, "ClusterViewMode#refreshViewMode() - RGI not longer valid!");
                                this.viewMode.setValue(0);
                            } else {
                                this.logChannel
                                    .log(1000000, "ClusterViewMode#refreshViewMode() - KDK not ready, stay in RGI");
                            }
                        } else if (this.favoredViewMode == 3) {
                            if (this.isMapReady()) {
                                this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - switch to MAP");
                                this.viewMode.setValue(3);
                            } else if (!this.rgiValid) {
                                this.logChannel
                                    .log(100000, "ClusterViewMode#refreshViewMode() - RGI not longer valid!");
                                this.viewMode.setValue(0);
                            } else {
                                this.logChannel
                                    .log(1000000, "ClusterViewMode#refreshViewMode() - MAP not ready, stay in RGI");
                            }
                        } else {
                            this.logChannel
                                .log(
                                    100000,
                                    "ClusterViewMode#refreshViewMode() - favoredViewMode: %1 not valid!",
                                    this.favoredViewMode
                                );
                        }
                        break;
                    case 2:
                        if (this.favoredViewMode == 0) {
                            this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - switch to COMPASS");
                            this.viewMode.setValue(0);
                        } else if (this.favoredViewMode == 1) {
                            if (this.rgiValid) {
                                this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - switch to RGI");
                                this.viewMode.setValue(1);
                            } else if (!this.isKDKReady()) {
                                this.logChannel
                                    .log(100000, "ClusterViewMode#refreshViewMode() - KDK not longer valid!");
                                this.viewMode.setValue(0);
                            } else {
                                this.logChannel
                                    .log(1000000, "ClusterViewMode#refreshViewMode() - RGI not ready, stay in MOST_KDK");
                            }
                        } else if (this.favoredViewMode == 2) {
                            if (!this.isKDKReady()) {
                                this.logChannel
                                    .log(100000, "ClusterViewMode#refreshViewMode() - KDK not longer valid!");
                                this.viewMode.setValue(0);
                            } else {
                                this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - stay in KDK");
                            }
                        } else if (this.favoredViewMode == 3) {
                            if (this.isMapReady()) {
                                this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - switch to MAP");
                                this.viewMode.setValue(3);
                            } else {
                                this.logChannel
                                    .log(100000, "ClusterViewMode#refreshViewMode() - MAP not longer valid!");
                                this.viewMode.setValue(0);
                            }
                        } else {
                            this.logChannel
                                .log(
                                    100000,
                                    "ClusterViewMode#refreshViewMode() - favoredViewMode: %1 not valid!",
                                    this.favoredViewMode
                                );
                        }
                        break;
                    case 3:
                        if (this.favoredViewMode == 1) {
                            if (this.rgiValid) {
                                this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - switch to RGI");
                                this.viewMode.setValue(1);
                            } else if (!this.isKDKReady()) {
                                this.logChannel
                                    .log(100000, "ClusterViewMode#refreshViewMode() - KDK not longer valid!");
                                this.viewMode.setValue(0);
                            } else {
                                this.logChannel
                                    .log(1000000, "ClusterViewMode#refreshViewMode() - RGI not ready, stay in MAP");
                            }
                        } else if (this.favoredViewMode == 2) {
                            if (this.isKDKReady()) {
                                this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - switch to KDK");
                                this.viewMode.setValue(2);
                            } else {
                                this.logChannel
                                    .log(100000, "ClusterViewMode#refreshViewMode() - KDK not longer valid!");
                                this.viewMode.setValue(0);
                            }
                        } else if (this.favoredViewMode == 3) {
                            if (!this.isMapReady()) {
                                this.logChannel
                                    .log(
                                        100000,
                                        "ClusterViewMode#refreshViewMode() - MOST not longer valid or map not ready! - gfxAvailable: %1, mapReady: %2, komoViewEnabled: %3",
                                        this.gfxAvailable,
                                        this.mapReady,
                                        this.komoViewEnabled
                                    );
                                this.viewMode.setValue(0);
                            } else {
                                this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - stay in MAP");
                            }
                        } else {
                            this.logChannel
                                .log(
                                    100000,
                                    "ClusterViewMode#refreshViewMode() - favoredViewMode: %1 not valid!",
                                    this.favoredViewMode
                                );
                        }
                        break;
                    default:
                        if (this.favoredViewMode == 1 && this.rgiValid) {
                            this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - switch to RGI");
                            this.viewMode.setValue(1);
                        } else if (this.favoredViewMode == 2 && this.isKDKReady()) {
                            this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - switch to KDK");
                            this.viewMode.setValue(2);
                        } else if (this.favoredViewMode == 3 && this.isMapReady()) {
                            this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - switch to MAP");
                            this.viewMode.setValue(3);
                        } else {
                            this.logChannel.log(1000000, "ClusterViewMode#refreshViewMode() - stay in COMPASS!");
                            this.viewMode.setValue(0);
                        }
                }
            }

            this.refreshMapVisibility();
            int j = this.viewMode.getValue();
            if (this.favoredViewModeReceived || j != i) {
                this.clusterService.refreshViewMode(j);
            }

            this.refreshDisplayContext();
            if (j != i) {
                this.clusterService.getKomoService().doFadeOut(1);
                this.refreshDisplayContext();
                this.refreshRgMode();
                this.clusterService.getKomoService().doFadeIn(1);
            }
        }
    }

    private boolean isMostViewMode(int i) {
        return Util.isClusterMapMOST(this.env.getFramework()) && i == 2 || i == 3;
    }

    protected void fadeOutFinished() {
        this.logChannel.log(1000000, "ClusterViewMode#fadeOutFinished()");
    }

    private void refreshDisplayContext() {
        if (ClusterProbe.on()) ClusterProbe.log("refreshDisplayContext: viewMode=" + this.viewMode.getValue());
        if (this.viewMode.getValue() == 2) {
            this.clusterService.switchDisplayContextKombi(9);
        } else if (this.viewMode.getValue() == 3) {
            this.clusterService.switchDisplayContextKombi(8);
        }
    }

    private void refreshMapVisibility() {
        if (Util.isClusterMapAvailable(this.env.getFramework())) {
            boolean flag = Util.isClusterMapMOST(this.env.getFramework()) ? this.dataRate == 2 : true;
            if (this.viewMode.getValue() == 3 && flag) {
                this.clusterService.showKombiMap(true);
            } else {
                this.clusterService.showKombiMap(false);
            }
        }
    }

    void refreshRgMode() {
        int i = this.viewMode.getValue();
        byte b0;
        switch (i) {
            case 0:
                b0 = 2;
                break;
            case 1:
                b0 = 0;
                break;
            case 2:
                b0 = 1;
                break;
            case 3:
                b0 = 3;
                break;
            default:
                this.logChannel.log(10000, "ClusterViewMode#refreshRgSelect() - unknown viewmode: %1", i);
                b0 = 2;
        }

        this.clusterService.getKomoService().setRgSelect(b0);
    }

    public static final String viewModeToString(int i) {
        String s;
        switch (i) {
            case 0:
                s = "COMPASS";
                break;
            case 1:
                s = "RGI";
                break;
            case 2:
                s = "KDK";
                break;
            case 3:
                s = "MAP";
                break;
            default:
                s = "UNKNOWN";
        }

        return s;
    }

    private boolean isKDKReady() {
        boolean flag = this.komoViewEnabled && this.komoViewVisible && this.env.getContainer().isRgActive();
        if (Util.isClusterMapMOST(this.env.getFramework())) {
            flag &= this.gfxAvailable;
        }

        if (!flag) {
            Buffer buffer = new Buffer(60);
            buffer.append(
                this.komoViewEnabled ? Util.INVALID_DISTANCE_NULL_AT_KOMBI_WITHOUT_TEXT_FOR_UNIT : "komoViewEnabled "
            );
            buffer.append(
                this.komoViewVisible ? Util.INVALID_DISTANCE_NULL_AT_KOMBI_WITHOUT_TEXT_FOR_UNIT : "komoViewVisible "
            );
            buffer.append(
                this.env.getContainer().isRgActive()
                    ? Util.INVALID_DISTANCE_NULL_AT_KOMBI_WITHOUT_TEXT_FOR_UNIT
                    : "isRgActive "
            );
            buffer.append(
                Util.isClusterMapMOST(this.env.getFramework()) && !this.gfxAvailable
                    ? "gfxAvailable"
                    : Util.INVALID_DISTANCE_NULL_AT_KOMBI_WITHOUT_TEXT_FOR_UNIT
            );
            this.logChannel.log(1000000, "ClusterViewMode#isKDKReady() - KDK is not ready because of: %1", buffer);
        }

        return flag;
    }

    private boolean isMapReady() {
        boolean flag = this.mapReady;
        if (Util.isClusterMapMOST(this.env.getFramework())) {
            flag &= this.gfxAvailable;
        }

        if (!flag) {
            Buffer buffer = new Buffer(30);
            buffer.append(this.mapReady ? Util.INVALID_DISTANCE_NULL_AT_KOMBI_WITHOUT_TEXT_FOR_UNIT : "mapReady ");
            buffer.append(
                Util.isClusterMapMOST(this.env.getFramework()) && !this.gfxAvailable
                    ? "gfxAvailable"
                    : Util.INVALID_DISTANCE_NULL_AT_KOMBI_WITHOUT_TEXT_FOR_UNIT
            );
            this.logChannel.log(1000000, "ClusterViewMode#isMapReady() - Map is not ready because of: %1", buffer);
        }

        return flag;
    }

    public String toString() {
        Buffer buffer = new Buffer();
        buffer.append("combiService: ").append(this.combiService != null ? "available" : "n/a").append('\n');
        buffer.append("currentViewMode: ").append(viewModeToString(this.viewMode.getValue())).append('\n');
        buffer.append("favoredViewMode: ").append(viewModeToString(this.favoredViewMode)).append('\n');
        buffer.append("rgiValid: ").append(this.rgiValid).append('\n');
        buffer.append("gfxAvailable: ").append(this.gfxAvailable).append('\n');
        buffer.append("mapReady: ").append(this.mapReady).append('\n');
        buffer.append("dataRate: ").append(this.dataRate).append('\n');
        buffer.append("komoViewEnabled: ").append(this.komoViewEnabled).append('\n');
        buffer.append("komoViewVisible: ").append(this.komoViewVisible).append('\n');
        return buffer.toString();
    }
}
