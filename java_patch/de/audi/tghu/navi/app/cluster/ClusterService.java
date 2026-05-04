package de.audi.tghu.navi.app.cluster;

import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.intercommunication.NaviMoKoKDKConstants;
import de.audi.atip.hmi.model.ModelGroup;
import de.audi.atip.hmi.modelaccess.ChoiceModelApp;
import de.audi.atip.hmi.modelaccess.MetricsModelApp;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNaviListener;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationInfo;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviDestination;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPSemiDynamicRouteInfo;
import de.audi.atip.interapp.combi.ddp2.CombiService;
import de.audi.atip.interapp.def.NullViewSizeManager;
import de.audi.atip.interapp.locationaccessor.IMyLocationAccessor;
import de.audi.atip.log.LogChannel;
import de.audi.atip.metrics.DateMetric;
import de.audi.atip.metrics.Distance;
import de.audi.atip.mmicombi.IViewSizeManager;
import de.audi.atip.power.PowerEventListener;
import de.audi.tghu.command.ICommandListFactory;
import de.audi.tghu.navi.app.NavigationEnv;
import de.audi.tghu.navi.app.command.DSIResponseContainer;
import java.lang.reflect.Field;
import de.audi.tghu.navi.app.OperationManager;
import de.audi.tghu.navi.app.SpeechManager;
import de.audi.tghu.navi.app.audio.AudioStateMachine;
import de.audi.tghu.navi.app.interapp.IViewSizeChangeHandler;
import de.audi.tghu.navi.app.interapp.NullViewSizeChangeHandler;
import de.audi.tghu.navi.app.map.AbstractMap;
import de.audi.tghu.navi.app.map.MapInterface;
import de.audi.tghu.navi.app.map.MapManager;
import de.audi.tghu.navi.app.map.handler.MapScaleHandler;
import de.audi.tghu.navi.app.map.handler.MapScaleInfo;
import de.audi.tghu.navi.app.map.handler.MapScaleTimer;
import de.audi.tghu.navi.app.map.routecalc.RcciEvent;
import de.audi.tghu.navi.app.rp.TripHandler$TripData;
import de.audi.tghu.navi.app.util.LocationFormatter;
import de.audi.tghu.navi.app.util.Util;
import de.audi.tghu.navi.app.util.addressformatting.AddressFormatter;
import de.audi.tghu.navi.app.util.addressformatting.LocationFormattingResponse;
import de.esolutions.fw.util.commons.Buffer;
import java.util.Date;
import org.dsi.ifc.global.NavLocation;
import org.dsi.ifc.komoview.RouteInfoElement;
import org.dsi.ifc.komoview.TrafficInfo;
import org.dsi.ifc.navigation.BapManeuverDescriptor;
import org.dsi.ifc.navigation.BapTurnToInfo;
import org.dsi.ifc.navigation.DistanceToNextManeuver;
import org.dsi.ifc.navigation.NavLaneGuidanceData;
import org.dsi.ifc.navigation.PosPosition;
import org.dsi.ifc.navigation.RgInfoForNextDestination;
import org.dsi.ifc.tmc.TmcMessage;

public class ClusterService implements NaviMoKoKDKConstants, PowerEventListener {
    public static final String EMPTY_STREET_LABEL = "---";
    protected LogChannel logChannel;
    protected final NavigationEnv env;
    private boolean rgiDataValid = false;
    private final DateMetric etaDateMetric;
    private final DateMetric rttDateMetric;
    private Distance distanceToManeuver = new Distance(0.0F, 1);
    private Distance distanceToDestination = new Distance(0.0F, 1);
    private ClusterViewMode clusterViewMode;
    protected ModelGroup travelParametersGroup;
    private ModelGroup nextManeuverGroup;
    private boolean showBargraph = false;
    private String turnToStreet = "";
    private boolean turnToStreetValid = false;
    protected KOMOService komoService = null;
    protected final CombiBAPListener combiBAPListener;
    private ClusterInputListener clusterInputListener = null;
    private ClusterKDKHandler clusterKDKHandler;
    private CombiBAPServiceNavi combiBAPServiceNavi;
    private CombiService combiDDP2ServiceNavi;
    protected final MapInterface mapInterface;
    protected RouteInfoElement followInfoRIE = null;
    private RgInfoForNextDestination rgInfoForNextDestination = null;
    private final Object komoDataRateMutex = new Object();
    private RouteInfoElement nextManeuverElement = null;
    private final BAPDistanceFormatter bapDistanceFormatter;
    private boolean operationStateIsKnownToTheKombi = false;
    private boolean dataConnectivityAvailable = false;
    private final MapScaleHandler mapScaleHandler;
    private final MapScaleTimer mapScaleTimer;
    private boolean satMapProviderChanged = true;

    public ClusterService(
        NavigationEnv navigationEnv,
        SpeechManager speechManager,
        OperationManager operationManager,
        AudioStateMachine audioStateMachine,
        MapManager mapManager,
        ICommandListFactory iCommandListFactory
    ) {
        this(
            navigationEnv,
            speechManager,
            operationManager,
            audioStateMachine,
            mapManager,
            iCommandListFactory,
            new NullViewSizeManager(navigationEnv.getClusterLogChannel()),
            new NullViewSizeChangeHandler()
        );
    }

    protected ClusterKDKHandler initClusterKDKHandler(IViewSizeChangeHandler iViewSizeChangeHandler) {
        return new ClusterKDKHandlerImpl(this.env, iViewSizeChangeHandler, this.combiBAPListener);
    }

    public ClusterService(
        NavigationEnv navigationEnv,
        SpeechManager speechManager,
        OperationManager operationManager,
        AudioStateMachine audioStateMachine,
        MapManager mapManager,
        ICommandListFactory iCommandListFactory,
        IViewSizeManager iViewSizeManager,
        IViewSizeChangeHandler iViewSizeChangeHandler
    ) {
        this.env = navigationEnv;
        this.mapInterface = mapManager.getMapInterface();
        this.logChannel = navigationEnv.getClusterLogChannel();
        this.bapDistanceFormatter = new BAPDistanceFormatter(this.logChannel);
        this.travelParametersGroup = new ModelGroup();
        this.nextManeuverGroup = new ModelGroup();
        this.etaDateMetric = new DateMetric(new Date(), 1);
        this.rttDateMetric = new DateMetric(new Date(), 3);
        this.followInfoRIE = new RouteInfoElement(
            null, null, 0, null, null, null, 0, null, null, null, null, new TrafficInfo(), 0, null, null, null, null, 0
        );
        this.combiBAPListener = this.initBAPListener(
            speechManager, operationManager, audioStateMachine, mapManager, iCommandListFactory, iViewSizeManager
        );
        this.clusterViewMode = new ClusterViewMode(navigationEnv, this);
        this.clusterKDKHandler = this.initClusterKDKHandler(iViewSizeChangeHandler);
        this.komoService = new KOMOService(navigationEnv, this, this.clusterKDKHandler);
        this.clusterInputListener = this.createClusterInputListener(navigationEnv);
        this.mapScaleHandler = new MapScaleHandler();
        this.mapScaleTimer = new MapScaleTimer(navigationEnv, this, this.mapScaleHandler);
        navigationEnv.getLabelModel(62).setText("");
        this.initializeModels();
    }

    private void initializeModels() {
        this.logChannel.log(10000000, "ClusterService#initializeModels() ");
        this.env.getLabelModel(71).setText("");
        this.env.getChoiceModel(69).setValue(0);
        this.turnToStreetValid = false;
        this.turnToStreet = "";
        this.env.getMetricsModel(66).setStatus(3);
        this.env.getMetricsModel(66).setMetric(this.etaDateMetric);
        this.env.getMetricsModel(63).setStatus(3);
        this.env.getMetricsModel(63).setMetric(this.distanceToDestination);
        this.travelParametersGroup.add(this.env.getMetricsModel(66));
        this.travelParametersGroup.add(this.env.getMetricsModel(63));
        this.env.getMetricsModel(64).setStatus(3);
        this.env.getMetricsModel(64).setMetric(this.distanceToManeuver);
        this.env.getChoiceModel(65).setValue(-1);
        this.showBargraph = false;
        this.nextManeuverGroup.add(this.env.getMetricsModel(64));
        this.nextManeuverGroup.add(this.env.getChoiceModel(65));
        if (Util.isClusterKDKOnly(this.env.getFramework())) {
            this.switchDisplayContextKombi(9);
        } else if (Util.isClusterMapAvailable(this.env.getFramework())) {
            this.switchDisplayContextKombi(8);
        }
    }

    protected CombiBAPListener initBAPListener(
        SpeechManager speechManager,
        OperationManager operationManager,
        AudioStateMachine audioStateMachine,
        MapManager mapManager,
        ICommandListFactory iCommandListFactory,
        IViewSizeManager iViewSizeManager
    ) {
        return new CombiBAPListener(
            this,
            this.logChannel,
            this.env,
            speechManager,
            operationManager,
            audioStateMachine,
            mapManager,
            iCommandListFactory,
            iViewSizeManager
        );
    }

    public synchronized void unitsChanged(TripHandler$TripData tripData) {
        this.logChannel.log(10000000, "ClusterService#unitsChanged()");
        this.refreshTravelParameters(tripData);
        this.refreshDistanceToNextManeuver();
        this.combiBAPListener.updateSemidynamicRouteGuidance();
        this.combiBAPListener.updateAltitude();
        this.combiBAPListener.onUnitsChanged();
    }

    public KOMOService getKomoService() {
        return this.komoService;
    }

    public ClusterViewMode getClusterViewMode() {
        return this.clusterViewMode;
    }

    public ClusterInputListener getClusterInputListener() {
        return this.clusterInputListener;
    }

    protected ClusterInputListener createClusterInputListener(NavigationEnv navigationEnv) {
        return new ClusterInputListener(navigationEnv, this);
    }

    public CombiBAPServiceNaviListener getCombiBAPListener() {
        return this.combiBAPListener;
    }

    public void setCombiBAPService(CombiBAPServiceNavi combiBAPServiceNavi) {
        this.logChannel.log(10000000, "ClusterService#setCombiBAPService()");
        this.combiBAPListener.setCombiService(combiBAPServiceNavi);
        this.combiBAPServiceNavi = combiBAPServiceNavi;
    }

    public void setCombiService(CombiService combiService) {
        this.logChannel.log(10000000, "ClusterService#setCombiService()");
        this.clusterViewMode.setCombiService(combiService);
        this.combiDDP2ServiceNavi = combiService;
    }

    public void setMOSTFrameVisible(boolean bl) {
        this.logChannel.log(10000000, "ClusterService#setMOSTFrameVisible( %1 )", bl);
        this.komoService.notifyVisibility(bl);
    }

    public boolean isLvdsMapVisible() {
        return this.combiBAPListener.lvdsMapVisible;
    }

    public void updateCurrentStreet(String string) {
        this.logChannel.log(100000000, "ClusterService#updateCurrentStreet( %1 )", string);
        String string1 = string;
        String string2 = string;
        String string3 = string;
        if (Util.isEmpty(string)) {
            this.logChannel.log(10000000, "ClusterService#updateCurrentStreet() - invalid currentStreet: %1", string);
            string2 = "";
            string1 = "";
            string3 = "---";
        }

        this.env.getLabelModel(62).setText(string3);
        this.komoService.setCurrentStreet(string1);
        this.combiBAPListener.setCurrentStreet(string2);
    }

    public void updateTurnToStreet(String string, boolean bl) {
        this.logChannel.log(100000000, "ClusterService#updateTurnToStreet( %1 )", string);
        this.turnToStreet = string;
        this.turnToStreetValid = !Util.isEmpty(string);
        this.env.getLabelModel(71).setText(string);
        this.refreshStreetMode();
    }

    private void refreshStreetMode() {
        this.logChannel
            .log(
                100000000,
                "ClusterService#refreshStreetMode() - turnToStreetValid: %1, showBargraph: %2 ",
                this.turnToStreetValid,
                this.showBargraph
            );
        ChoiceModelApp choiceModelApp = this.env.getChoiceModel(69);
        if (this.turnToStreetValid && this.showBargraph) {
            choiceModelApp.setValue(1);
            this.komoService.setTurnToStreet(this.turnToStreet, "");
        } else {
            choiceModelApp.setValue(0);
            this.komoService.setTurnToStreet("", "");
        }
    }

    public void refreshDistanceToNextManeuver() {
        DistanceToNextManeuver distanceToNextManeuver = this.env.getContainer().getDistanceToNextManeuver();
        this.refreshDistanceToNextManeuver(distanceToNextManeuver);
    }

    private int convertBAP2KOMODistanceUnit(int i) {
        switch (i) {
            case 0:
                return 1;
            case 1:
            case 6:
                return 2;
            case 2:
                return 3;
            case 3:
                return 4;
            case 4:
            case 7:
                return 0;
            case 5:
                return 5;
            case 255:
            default:
                this.logChannel
                    .log(
                        100000,
                        "ClusterService#convertBAP2KOMOUnit() - unknown or not convertable BAP unit: %1",
                        (long)i
                    );
                return 255;
        }
    }

    protected void refreshDistanceToNextManeuver(DistanceToNextManeuver distanceToNextManeuver) {
        if (distanceToNextManeuver == null) {
            this.logChannel
                .log(1000000, "ClusterService#refreshDistanceToNextManeuver() - distanceToNextManeuver is null! ");
        } else {
            this.logChannel
                .log(
                    100000000,
                    "ClusterService#refreshDistanceToNextManeuver() - distanceToNextManeuver: %1",
                    distanceToNextManeuver
                );
            boolean bl = distanceToNextManeuver.showDistance;
            int i = distanceToNextManeuver.distance;
            this.showBargraph = distanceToNextManeuver.showBargraph;
            int j = distanceToNextManeuver.bargraph;
            long l;
            int k;
            boolean bl1;
            if (i > 0) {
                BAPDistanceFormatter$BAPDistance bAPDistance = this.bapDistanceFormatter
                    .formatDistanceToTurn(i, Distance.getSystemUnit() == 1);
                l = bAPDistance.getValue();
                k = this.convertBAP2KOMODistanceUnit(bAPDistance.getUnit());
                bl1 = !this.showBargraph;
            } else {
                l = -1L;
                k = 255;
                bl1 = false;
            }

            MetricsModelApp metricsModelApp = this.env.getMetricsModel(64);
            if (bl && i > 0) {
                this.distanceToManeuver.setValue(i / 1000.0F);
                metricsModelApp.setMetric(this.distanceToManeuver);
            }

            if (bl && i > 0) {
                Util.setModelStatus(metricsModelApp, 1);
            } else {
                Util.setModelStatus(metricsModelApp, 3);
            }

            int m = this.showBargraph ? j : -1;
            this.env.getChoiceModel(65).setValue(m);
            this.refreshStreetMode();
            this.nextManeuverGroup.flush();
            this.refreshDistanceToNextManeuverMOST(l, k, bl1, bl);
            this.combiBAPListener.setDistanceToNextManeuver(i, this.showBargraph, j);
        }
    }

    public void refreshTravelParameters(TripHandler$TripData tripData) {
        this.logChannel.log(100000000, "ClusterService#refreshTravelParameters()");
        if (tripData.etaModeActive) {
            this.updateArrivalTime(tripData.etaValid, tripData.etaToNextDestination, tripData.isTimeZoneOffset);
        } else {
            this.updateRemainingTravelTime(tripData.etaValid, tripData.timeToNextDestination * 1000L);
        }

        this.followInfoRIE.destinationIndex = this.getDestIndex();
        this.updateDistanceToDestination(tripData.distanceToNextDestination, this.followInfoRIE.destinationIndex == 0);
        this.updateKOMOFollowInfo();
        this.travelParametersGroup.flush();
    }

    private void clearRouteInfoElement(RouteInfoElement routeInfoElement) {
        if (routeInfoElement != null) {
            routeInfoElement.distanceToElement = "";
            routeInfoElement.estimatedTimeToElement = "--:--";
            routeInfoElement.routeInfoElementType = 0;
            routeInfoElement.elementIconIDs = null;
            routeInfoElement.prio1EventText = null;
            routeInfoElement.streetIconText = null;
            routeInfoElement.streetIconID = 0;
            routeInfoElement.exitNumber = null;
            routeInfoElement.turnToStreet = null;
            routeInfoElement.pOIElementNames = null;
            routeInfoElement.maneuverDescriptor = null;
            if (routeInfoElement.trafficInfo != null) {
                routeInfoElement.trafficInfo.trafficOffset = null;
                routeInfoElement.trafficInfo.trafficOffsetAffix = null;
                routeInfoElement.trafficInfo.affixPlacementBefore = false;
            }

            routeInfoElement.destinationIndex = 0;
            routeInfoElement.signPostInfo = null;
            routeInfoElement.distanceToManeuver = null;
            routeInfoElement.estimatedTimeToManeuver = null;
            routeInfoElement.streetCardinalDirection = null;
            routeInfoElement.exitIconId = 0;
        }
    }

    protected void refreshDistanceToNextManeuverMOST(long l, int i, boolean bl, boolean bl1) {
        this.komoService.setDistanceToNextManeuver(l, i, bl);
    }

    protected int getDestIndex() {
        int i = this.mapInterface.getMap().getNaviInterface().getRouteListLength();
        int j = this.mapInterface.getMap().getNaviInterface().getIndexOfCurrentDestination();
        return j + 1 < i ? 0 : -1;
    }

    protected void updateArrivalTime(boolean bl, long l, boolean bl1) {
        this.logChannel.log(100000000, "ClusterService#updateArrivalTime( %1, %2 )", bl, l);
        MetricsModelApp metricsModelApp = this.env.getMetricsModel(66);
        String string;
        if (bl) {
            this.etaDateMetric.setDate(l);
            metricsModelApp.setMetric(this.etaDateMetric);
            Util.setModelStatus(metricsModelApp, 1);
            string = Util.formatTime(l, 2, this.env);
        } else {
            this.logChannel.log(10000000, "ClusterService#updateArrivalTime() - invalid flag for ETA set! ");
            Util.setModelStatus(metricsModelApp, 3);
            string = "--:--";
        }

        int i = KOMOService.convertTimeFormatToKOMO(DateMetric.timeFormat);
        KOMOTime kOMOTime = KOMOService.convertTimeToKOMO(l);
        this.followInfoRIE.estimatedTimeToElement = string;
        this.komoService.setETA(i, kOMOTime.day, kOMOTime.hour, kOMOTime.min, bl, bl1);
        this.combiBAPListener.setRgTimeToNextDestination(l, true, bl);
    }

    protected void updateRemainingTravelTime(boolean bl, long l) {
        this.logChannel.log(100000000, "ClusterService#RemainingTravelTime( %1, %2 )", bl, l);
        MetricsModelApp metricsModelApp = this.env.getMetricsModel(66);
        if (bl) {
            this.rttDateMetric.setDate(l);
            metricsModelApp.setMetric(this.rttDateMetric);
            Util.setModelStatus(metricsModelApp, 1);
        } else {
            this.logChannel.log(10000000, "ClusterService#updateArrivalTime() - invalid flag for ETA set! ");
            Util.setModelStatus(metricsModelApp, 3);
        }

        KOMOTime kOMOTime = KOMOService.convertDurationToKOMO(l);
        this.komoService.setRTT(kOMOTime.hour, kOMOTime.min, bl);
        this.combiBAPListener.setRgTimeToNextDestination(l, false, bl);
    }

    protected void updateDistanceToDestination(int i, boolean bl) {
        this.logChannel.log(100000000, "ClusterService#updateDistanceToDestination( %1 )", (long)i);
        long l = -1L;
        int j = -1;
        MetricsModelApp metricsModelApp = this.env.getMetricsModel(63);
        String string;
        boolean bl1;
        if (i > 0) {
            this.distanceToDestination.setValue(i / 1000.0F);
            metricsModelApp.setMetric(this.distanceToDestination);
            Util.setModelStatus(metricsModelApp, 1);
            string = Util.formatDistance(i, 1, 2, "---");
            BAPDistanceFormatter$BAPDistance bAPDistance = this.bapDistanceFormatter
                .formatDistanceToDestination(i, Distance.getSystemUnit() == 1);
            l = bAPDistance.getValue();
            j = this.convertBAP2KOMODistanceUnit(bAPDistance.getUnit());
            bl1 = true;
        } else {
            this.logChannel
                .log(10000000, "ClusterService#updateDistanceToDestination() - invalid distanceToDestination! ");
            Util.setModelStatus(metricsModelApp, 3);
            string = "";
            bl1 = false;
        }

        this.followInfoRIE.distanceToElement = string;
        this.komoService.setDistanceToDestination(l, j, bl1);
        this.combiBAPListener.setRgDistanceToNextDestination(i, bl);
    }

    public void updateSoPosPosition(PosPosition posPosition) {
        this.logChannel.log(100000000, "ClusterService#updateSoPosPosition( %1 )", posPosition);
        short s = 0;
        short t = 255;
        int i = -1;
        if (posPosition != null) {
            s = (short)posPosition.getDirectionAngle();
            t = (short)posPosition.getDirectionSymbolic();
            i = posPosition.getState();
        } else {
            this.logChannel.log(100000, "ClusterService#updateSoPosPosition() - invalid soPosPosition!");
        }

        this.combiBAPListener.setVehicleHeading(s, t);
        this.combiBAPListener.setInfoStateGPS(i);
    }

    public void updateSoPosPositionDescription(NavLocation navLocation) {
        this.logChannel.log(100000000, "ClusterService#updateSoPosPositionDescription( %1 )", navLocation);
        String string = LocationFormatter.formatCity(navLocation);
        this.komoService.setCityName(string);
    }

    public void updateRgDirectionToNextDestination(short s) {
        this.logChannel.log(100000000, "ClusterService#updateRgDirectionToNextDestination( %1 )", (long)s);
    }

    public void updateRGIString(short[] s) {
        if (s != null && s.length > 0) {
            this.rgiDataValid = true;
            this.env.getDataModel(68).set(s);
        } else {
            this.rgiDataValid = false;
            this.logChannel.log(10000000, "ClusterService#updateRGIData() - invalid RGI data ");
        }

        this.refreshRGIValid();
    }

    public void updateRgActive(boolean bl) {
        this.logChannel.log(100000000, "ClusterService#updateRgActive( %1 )", bl);
        this.refreshRGIValid();
        this.clusterViewMode.refreshRGState();
        if (!bl) {
            this.initializeModels();
            this.updateTurnToStreet("", false);
            this.clearRouteInfoElement(this.followInfoRIE);
            this.clearRouteInfoElement(this.nextManeuverElement);
            this.updateDestinationInfo(null, 0, 0);
        }

        this.clusterKDKHandler.updateRgActive(bl);
        this.combiBAPListener.setRgActive(bl);
    }

    public void updateNavState(int i, int j) {
        this.logChannel.log(10000000, "ClusterService#updateNavState( %1, %2 )", i, j);
        if (this.combiDDP2ServiceNavi != null) {
            this.combiDDP2ServiceNavi.updateNavInitialized(i, j);
        }
    }

    private void refreshRGIValid() {
        boolean bl = this.env.getContainer().isRgActive();
        boolean bl1 = bl && this.rgiDataValid;
        this.logChannel
            .log(100000000, "ClusterService#refreshRGIValid() - rgActive: %1, rgiDataValid: %2", bl, this.rgiDataValid);
        this.clusterViewMode.setRGIValid(bl1);
    }

    public void refreshViewMode(int i) {
        this.combiBAPListener.setViewMode(i);
    }

    public void updateLaneGuidance(NavLaneGuidanceData[] navLaneGuidanceDatas, boolean bl) {
        this.combiBAPListener.setLaneGuidance(navLaneGuidanceDatas, bl);
    }

    public void updateManeuverDescriptor(BapManeuverDescriptor[] bapManeuverDescriptors) {
        this.rgiDataValid = bapManeuverDescriptors != null && bapManeuverDescriptors.length > 0;
        this.refreshRGIValid();
        this.combiBAPListener.setManeuverDescriptor(bapManeuverDescriptors);
    }

    public void updateManeuverDescriptor(BapManeuverDescriptor[] bapManeuverDescriptors, int i) {
        this.rgiDataValid = bapManeuverDescriptors != null && bapManeuverDescriptors.length > 0;
        this.refreshRGIValid();
        this.combiBAPListener.setManeuverDescriptor(bapManeuverDescriptors, i);
    }


    public String toString() {
        Buffer buffer = new Buffer();
        buffer.append(this.komoService);
        buffer.append(this.clusterViewMode);
        return buffer.toString();
    }

    public void updateBapTurnToInfo(BapTurnToInfo[] bapTurnToInfos) {
        this.combiBAPListener.setTurnToInfo(bapTurnToInfos);
    }

    public void updateInfoStatesGPS(int i) {
        this.combiBAPListener.setInfoStateGPS(i);
    }

    private boolean initScreenNeededOnKombi() {
        boolean bl = this.env.getContainer().getNavstateOfOperation() == 5;
        boolean bl1 = Util.isClusterMapAvailable(this.env.getFramework());
        boolean bl2 = false;
        AbstractMap abstractMap = this.mapInterface.getKombiMap();
        if (abstractMap != null && abstractMap.isInitialized()) {
            bl2 = true;
        }

        return bl && !bl2 && bl1 ? true : !this.operationStateIsKnownToTheKombi;
    }

    public synchronized void updateOperationState(int i) {
        this.logChannel.log(1000000, "ClusterService#updateOperationState( %1 )", (long)i);
        this.operationStateIsKnownToTheKombi = this.combiBAPListener.setInfoStateNavi(i);
        this.combiBAPListener.forceShowInitScreen(this.initScreenNeededOnKombi());
        if (this.combiBAPListener.lvdsMapVisible && this.env.getContainer().getNavstateOfOperation() == 5) {
            this.showKombiMap(true);
        }
    }

    public void updateXUrgentMessages(TmcMessage[] tmcMessages) {
        this.combiBAPListener.setXUrgentMessages(tmcMessages);
    }

    public void updateMessagesOnRoute(TmcMessage[] tmcMessages) {
        this.combiBAPListener.setMessagesOnRoute(tmcMessages);
    }

    public void setRouteGuidanceAborted() {
        this.combiBAPListener.setRouteGuidanceAborted();
    }

    public void updateRgInfoForNextDestination(RgInfoForNextDestination rgInfoForNextDestination) {
        this.logChannel.log(100000000, "ClusterService#updateRgInfoForNextDestination(%1)", rgInfoForNextDestination);
        this.rgInfoForNextDestination = rgInfoForNextDestination;
        this.updateRgDirectionToNextDestination(rgInfoForNextDestination.getDirectionToNextDest());
    }

    public void updateDistanceToNextManeuver(DistanceToNextManeuver distanceToNextManeuver) {
        this.refreshDistanceToNextManeuver(distanceToNextManeuver);
    }

    public synchronized void updateKombiMapReady(boolean bl) {
        this.logChannel.log(1000000, "ClusterService#updateKombiMapReady( %1 )", bl);
        if (bl) {
            this.switchDisplayContextKombi(8);
            if (Util.isClusterMapAlwaysOn()) {
                this.combiBAPListener.setMainMapVisibility(true);
            }

            if (this.combiBAPListener.lvdsMapVisible) {
                this.showKombiMap(true);
            }
        }

        this.combiBAPListener.forceShowInitScreen(this.initScreenNeededOnKombi());
        this.getClusterViewMode().setKombiMapReady(bl);
    }

    public void showKombiMap(boolean bl) {
        this.logChannel.log(1000000, "ClusterService#showKombiMap( %1 )", bl);
        this.mapInterface.showKombiMap(bl);
    }

    public void setSupplementaryMap(int i, boolean bl) {
        this.logChannel
            .log(10000000, "ClusterService#setSupplementaryMap() - supplementaryMapView: %2, visible: %1", bl, i);
        if (i != 1 && bl) {
            this.logChannel
                .log(
                    100000,
                    "ClusterService#setSupplementaryMap() - got request to show supplementary map although not available or unimplemented"
                );
        }

        this.setKDKVisibility(bl);
    }

    public void switchDisplayContextKombi(int i) {
        this.logChannel.log(1000000, "ClusterService#switchDisplayContextKombi( %1 )", (long)i);
        this.mapInterface.switchDisplayContextKombi(i);
    }

    public void setKOMODataRate(int i) {
        if (Util.isClusterMapMOST(this.env.getFramework())
                || Util.isClusterMapFPK(this.env.getFramework())) {
            this.setKOMODataRate(i, true);
        }
    }

    public int getKOMODataRate() {
        this.logChannel.log(100000000, "ClusterService#getKOMODataRate()");
        ChoiceModelApp choiceModelApp = this.env.getChoiceModel(1, 168);
        int i = choiceModelApp.getHints();
        boolean bl = (i & 2) == 2;
        boolean bl1 = (i & 1) == 1;
        if (bl) {
            if (bl1) {
                this.logChannel
                    .log(
                        100000,
                        "ClusterService#getKOMODataRate() - undefined state: full and reduced framerate -> assuming full framerate"
                    );
            }

            return 2;
        } else {
            return bl1 ? 1 : 0;
        }
    }

    public void reSyncKOMO() {
        this.logChannel.log(1000000, "ClusterService#reSyncKOMO()");
        synchronized (this.komoDataRateMutex) {
            int i = this.getKOMODataRate();
            this.setKOMODataRate(0);
            this.setKOMODataRate(i);
            this.clusterViewMode.refreshRgMode();
        }
    }

    private void setKOMODataRate(int i, boolean bl) {
        this.logChannel.log(1000000, "ClusterService#setKOMODataRate( %1 )", (long)i);
        synchronized (this.komoDataRateMutex) {
            ChoiceModelApp choiceModelApp = this.env.getChoiceModel(1, 168);
            if (i == 2) {
                choiceModelApp.removeHint(1);
                choiceModelApp.addHint(2);
            } else if (i == 1) {
                choiceModelApp.removeHint(2);
                choiceModelApp.addHint(1);
            } else {
                choiceModelApp.removeHint(2);
                choiceModelApp.removeHint(1);
            }

            if (bl) {
                choiceModelApp.publishHints();
            }
        }
    }

    public void onAutoZoomStateChanged(boolean bl) {
        this.combiBAPListener.setAutoZoomActive(bl);
    }

    public void updateKOMOFollowInfo() {
        this.logChannel.log(100000000, "ClusterService#updateKOMOFollowInfo())");
        if (this.komoService != null) {
            try {
                this.logChannel
                    .log(
                        100000000,
                        "ClusterService#updateKOMOFollowInfo() - followInfoRIE: %1, nextManeuverElement: %2",
                        this.followInfoRIE,
                        this.nextManeuverElement
                    );
                this.komoService.setRouteInfo(new RouteInfoElement[]{this.followInfoRIE, this.nextManeuverElement});
            } catch (Exception exception) {
                this.logChannel.log(10000, "ClusterService#updateFollowInfo() - ERROR=%1", exception);
            }
        }
    }

    public void updateAltitude(int i) {
        this.logChannel.log(100000000, "ClusterService#updateAltitude( %1 )", (long)i);
        this.combiBAPListener.setAltitude(i);
    }

    private String getDestinationDescription4BAP(NavLocation navLocation) {
        if (navLocation != null && navLocation.isPositionValid()) {
            try {
                LocationFormattingResponse locationFormattingResponse = AddressFormatter.formatTwoLines(
                    navLocation, this.env
                );
                String string = locationFormattingResponse.getFirstLineAsText();
                return string != null ? string : "";
            } catch (Exception exception) {
                this.env
                    .getLogChannel()
                    .log(
                        100000,
                        "Util#getString4BAPFromNavLocation - got an exception from AddressFormatter#formatTwoLines: %1",
                        (Throwable)exception
                    );
                return "";
            }
        } else {
            return "";
        }
    }

    private CombiBAPNaviDestination getBAPNaviDestFromLocation(NavLocation navLocation) {
        if (navLocation == null) {
            return new CombiBAPNaviDestination();
        } else {
            IMyLocationAccessor iMyLocationAccessor = Util.getLocationAccessor(navLocation);
            String string = this.getDestinationDescription4BAP(navLocation);
            if (Util.isEmpty(string)) {
                string = Util.isEmpty(iMyLocationAccessor.getPoiName()) ? null : iMyLocationAccessor.getPoiName();
            }

            return new CombiBAPNaviDestination(
                null,
                null,
                Util.isEmpty(navLocation.getStreet()) ? null : navLocation.getStreet(),
                Util.isEmpty(navLocation.getHousenumber()) ? null : navLocation.getHousenumber(),
                Util.isEmpty(navLocation.getTown()) ? null : navLocation.getTown(),
                Util.isEmpty(navLocation.getTownRefinement()) ? null : navLocation.getTownRefinement(),
                Util.isEmpty(iMyLocationAccessor.getState()) ? null : iMyLocationAccessor.getState(),
                Util.isEmpty(navLocation.getZipCode()) ? null : navLocation.getZipCode(),
                Util.isEmpty(navLocation.getCountry()) ? null : navLocation.getCountry(),
                wgs84ToDeg(navLocation.getLatitude()),
                wgs84ToDeg(navLocation.getLongitude()),
                iMyLocationAccessor.getType() == 1 ? 255 : 0,
                string,
                Util.isEmpty(iMyLocationAccessor.getPoiCategory()) ? null : iMyLocationAccessor.getPoiCategory(),
                255
            );
        }
    }

    public void updateDestinationInfo(NavLocation navLocation, int i, int j) {
        if (this.logChannel.isDebug2()) {
            this.logChannel
                .log(
                    100000000,
                    "ClusterService#updateDestinationInfo() - noOfStopovers: %2, noOfNextStopover: %3, nextDestination: %1",
                    LocationFormatter.formatLocationShort(navLocation),
                    i,
                    j
                );
        }

        CombiBAPNaviDestination combiBAPNaviDestination = this.getBAPNaviDestFromLocation(navLocation);
        CombiBAPDestinationInfo combiBAPDestinationInfo;
        if (navLocation != null) {
            combiBAPDestinationInfo = new CombiBAPDestinationInfo(combiBAPNaviDestination);
            combiBAPDestinationInfo.setStopoverInformation(i, j);
        } else {
            combiBAPDestinationInfo = new CombiBAPDestinationInfo(combiBAPNaviDestination);
            combiBAPDestinationInfo.setStopoverInformation(0, 0);
        }

        this.combiBAPListener.setDestinationInfo(combiBAPDestinationInfo);
    }

    public void updateSemidynamicRouteGuidance(RcciEvent rcciEvent) {
        this.logChannel
            .log(
                10000000,
                "ClusterService#updateSemidynamicRouteGuidance() - TrafficImpactOnCurrentRoute: %1, delay: %2",
                rcciEvent.hasTrafficImpactOnCurrentRoute(),
                rcciEvent.delay
            );
        KOMOTime kOMOTime = KOMOService.convertDurationToKOMO(rcciEvent.delay);
        short s = kOMOTime.min;
        short t = kOMOTime.hour;
        short u = kOMOTime.day;
        byte b = 0;
        if (this.rgInfoForNextDestination != null) {
            b = 0;
        }

        CombiBAPSemiDynamicRouteInfo combiBAPSemiDynamicRouteInfo;
        if (rcciEvent.hasBetterRoute && rcciEvent.origin != null) {
            Util.formatDistance((int)rcciEvent.origin.newRoute.distance, 1);
            long l = (long)Util.getFormattedDistance();
            int i = Util.getFormattedUnit();
            KOMOTime kOMOTime1 = KOMOService.convertTimeToKOMO(
                this.env.getFramework().getKombiTime() + rcciEvent.origin.newRoute.getEtaWithSpeedAndFlow() + b
            );
            combiBAPSemiDynamicRouteInfo = new CombiBAPSemiDynamicRouteInfo(
                rcciEvent.hasTrafficImpactOnCurrentRoute(),
                rcciEvent.hasBetterRoute,
                t + u * 24,
                s,
                l,
                i,
                0,
                1,
                DateMetric.timeFormat == 10 ? 0 : 1,
                kOMOTime1.min,
                kOMOTime1.hour,
                kOMOTime1.day,
                kOMOTime1.month,
                kOMOTime1.year
            );
        } else {
            combiBAPSemiDynamicRouteInfo = new CombiBAPSemiDynamicRouteInfo(
                rcciEvent.hasTrafficImpactOnCurrentRoute(), rcciEvent.hasBetterRoute, t + u * 24, s
            );
        }

        this.combiBAPListener.setSemidynamicRouteGuidance(combiBAPSemiDynamicRouteInfo);
        this.komoService.setSemiDynRoute(rcciEvent.hasBetterRoute);
        if (rcciEvent.hasTrafficImpactOnCurrentRoute()) {
            this.komoService
                .setTrafficOffset(KOMOService.convertTimeFormatToKOMO(DateMetric.timeFormat), u, t, s, true);
            if (rcciEvent.reliable) {
                this.followInfoRIE.trafficInfo.trafficOffset = Util.formatTrafficOffsetDuration(
                    rcciEvent.delay, 2, this.env
                );
            } else {
                this.followInfoRIE.trafficInfo.trafficOffset = "";
                this.followInfoRIE.estimatedTimeToElement = "--:--";
            }

            this.followInfoRIE.trafficInfo.trafficOffsetAffix = this.env.getTranslatedText(49, "incl.");
        } else {
            this.komoService
                .setTrafficOffset(KOMOService.convertTimeFormatToKOMO(DateMetric.timeFormat), u, t, s, false);
            this.followInfoRIE.trafficInfo.trafficOffset = "";
            this.followInfoRIE.trafficInfo.trafficOffsetAffix = "";
        }

        this.updateKOMOFollowInfo();
    }

    public void updateExitView(int i) {
        this.logChannel.log(100000000, "ClusterService#updateExitView( %1 )", (long)i);
        this.combiBAPListener.setExitView(i);
    }


    public void notifyPowerListenerOnEnterState(int i, int j) {
        this.clusterKDKHandler.notifyPowerListenerOnEnterState(i, j);
    }


    public void notifyPowerListenerOnExitState(int i, int j) {
        this.clusterKDKHandler.notifyPowerListenerOnExitState(i, j);
    }


    public void notifyPowerTriggerAction(int i, int j) {
        this.clusterKDKHandler.notifyPowerTriggerAction(i, j);
    }


    public void updateClampState(boolean bl, boolean bl1, boolean bl2, boolean bl3) {
        this.clusterKDKHandler.updateClampState(bl, bl1, bl2, bl3);
    }

    public void showKombiSplashScreen(boolean bl) {
        this.combiBAPListener.showSplashScreen(bl);
    }

    public void updateBapManeuverState(int i) {
        this.combiBAPListener.updateBapManeuverState(i);
    }

    public void applySetupHandlerSettings() {
        this.logChannel.log(10000000, "ClusterService#applySetupHandlerSettings( )");
        this.combiBAPListener.initFromSetup();
    }

    public void setKDKVisibility(boolean bl) {
        this.clusterKDKHandler.setKDKVisibility(bl);
    }

    public void updateManoeuvreViewActive(int i) {
    }

    public void onMagnificationChanged(int i) {
        this.combiBAPListener.onMapScaleChanged(i);
        AbstractMap abstractMap = this.mapInterface.getKombiMap();
        if (abstractMap != null) {
            float[] f = abstractMap.getZoomHandler().getZoomList();
            MapScaleInfo mapScaleInfo = this.mapScaleHandler.createMapScaleInfo(i, f);
            if (!mapScaleInfo.equals(this.mapScaleTimer.mapScaleInfo)) {
                this.mapScaleTimer.restart(mapScaleInfo);
            }
        }
    }

    public void updateNextManeuver(RouteInfoElement routeInfoElement) {
        this.nextManeuverElement = routeInfoElement;
        this.updateKOMOFollowInfo();
    }

    public static float wgs84ToDeg(int i) {
        return i / 1.1930464E7F;
    }

    public void setSupportedSupplementaryMapView(int i) {
        this.logChannel.log(10000000, "ClusterService#setSupportedSupplementaryMapView()");
    }

    public void updateOnlineNavigationState() {
        int i;
        if (this.mapInterface.getKombiMap() != null && this.mapInterface.getKombiMap().getSetup() != null) {
            i = this.mapInterface.getKombiMap().getSetup().getMapRepresentation();
        } else {
            this.logChannel
                .log(
                    10000000,
                    "ClusterService#updateOnlineNavigationState() - Kombi map does not exist or not initialized yet!"
                );
            i = 0;
        }

        int j = this.env.getChoiceModel(162).getValue();
        this.logChannel
            .log(
                10000000,
                "ClusterService#updateOnlineNavigationState() - mapRepresentation: %1, bufferProgress: %2, dataConnectivityAvailable: %3",
                i,
                j,
                true
            );
        this.combiBAPListener.setOnlineNavigationState(i == 1 && !this.hasSatMapProviderChanged(), j, true);
    }

    public void cleanup() {
        this.clusterKDKHandler.cleanup();
        this.clusterInputListener.cleanup();
        this.mapScaleTimer.cancel();
    }

    public void updateGALState(boolean bl) {
        this.combiBAPListener.setGALState(bl);
    }

    public void updateOnlineConnectionState(boolean bl) {
        this.dataConnectivityAvailable = bl;
        this.updateOnlineNavigationState();
    }

    public void updateMapScale(int i, int j, boolean bl) {
        this.logChannel.log(100000000, "ClusterService#updateMapScale( %1, %2, %3 )", i, j, bl);
        boolean[] bl1 = new boolean[]{false};
        boolean[] bl2 = new boolean[]{false};
        this.komoService.setMapScale(0, 0, bl1, i, j, bl2, bl);
    }

    public void setHomeAddress(NavLocation navLocation) {
        this.logChannel
            .log(
                10000000,
                "ClusterService#updateHomeAddress() - homeAddress: %1",
                LocationFormatter.formatLocationShort(navLocation)
            );
        if (navLocation == null) {
            this.combiBAPListener.setHomeAddress(null);
        } else {
            this.combiBAPListener.setHomeAddress(this.getBAPNaviDestFromLocation(navLocation));
        }
    }

    public void setProviderChangeFlag(boolean bl) {
        this.satMapProviderChanged = bl;
    }

    private boolean hasSatMapProviderChanged() {
        return this.satMapProviderChanged;
    }

    /* === CarPlay hook accessors (avoid reflection in BAPBridge) === */

    public DSIResponseContainer getDSIResponseContainer() {
        return this.env.getContainer();
    }

    public void triggerRefreshRGIValid() {
        this.refreshRGIValid();
    }

    public CombiBAPServiceNavi getCombiBAPListenerCombiService() {
        return this.combiBAPListener.combiservice;
    }

    public void setCombiBAPListenerCombiService(CombiBAPServiceNavi svc) {
        this.combiBAPListener.combiservice = svc;
    }

    public String getCombiBAPListenerDiagnostics() {
        try {
            Field fStatus = CombiBAPListener.class.getDeclaredField("rgStatus");
            fStatus.setAccessible(true);
            Field fType = CombiBAPListener.class.getDeclaredField("rgType");
            fType.setAccessible(true);
            Field fPhone = CombiBAPListener.class.getDeclaredField("naviIsRunningOnSmartphone");
            fPhone.setAccessible(true);
            return "rgStatus=" + fStatus.getInt(this.combiBAPListener)
                + " rgType=" + fType.getInt(this.combiBAPListener)
                + " naviOnPhone=" + fPhone.getBoolean(this.combiBAPListener);
        } catch (Exception e) {
            return "diagnostics unavailable";
        }
    }

    public void updateKDKRgActive(boolean active) {
        this.clusterKDKHandler.updateRgActive(active);
    }

    public void setValidRGTypeReceived(boolean valid) {
        this.combiBAPListener.validRGTypeReceived(valid);
    }

    public int getKOMOHintsRaw() {
        return this.env.getChoiceModel(1, 168).getHints();
    }

    /** Compact diagnostic string for ClusterViewMode state (video pipeline debugging). */
    public String getClusterViewModeDiag() {
        try {
            java.lang.reflect.Field fGfx = ClusterViewMode.class.getDeclaredField("gfxAvailable");
            fGfx.setAccessible(true);
            java.lang.reflect.Field fEnabled = ClusterViewMode.class.getDeclaredField("komoViewEnabled");
            fEnabled.setAccessible(true);
            java.lang.reflect.Field fVisible = ClusterViewMode.class.getDeclaredField("komoViewVisible");
            fVisible.setAccessible(true);
            java.lang.reflect.Field fRate = ClusterViewMode.class.getDeclaredField("dataRate");
            fRate.setAccessible(true);
            java.lang.reflect.Field fMapReady = ClusterViewMode.class.getDeclaredField("mapReady");
            fMapReady.setAccessible(true);
            return "gfx=" + fGfx.getBoolean(this.clusterViewMode)
                + " en=" + fEnabled.getBoolean(this.clusterViewMode)
                + " vis=" + fVisible.getBoolean(this.clusterViewMode)
                + " rate=" + fRate.getInt(this.clusterViewMode)
                + " mapRdy=" + fMapReady.getBoolean(this.clusterViewMode);
        } catch (Exception e) {
            return "diag-err";
        }
    }

    private Field findFieldInHierarchy(Class clazz, String name) {
        Class c = clazz;
        while (c != null) {
            try {
                Field f = c.getDeclaredField(name);
                f.setAccessible(true);
                return f;
            } catch (NoSuchFieldException e) {
                c = c.getSuperclass();
            } catch (Throwable t) {
                return null;
            }
        }
        return null;
    }

    private String getDisplayManagerInitState(Object dm) {
        if (dm == null) return "init=n/a dsi=n/a";
        try {
            Field fInit = this.findFieldInHierarchy(dm.getClass(), "initialized");
            Field fDsi = this.findFieldInHierarchy(dm.getClass(), "dsiDispMgmt");
            String init = "n/a";
            String dsi = "n/a";
            if (fInit != null) {
                init = String.valueOf(fInit.get(dm));
            }
            if (fDsi != null) {
                dsi = String.valueOf(fDsi.get(dm) != null);
            }
            return "init=" + init + " dsi=" + dsi;
        } catch (Throwable t) {
            return "init-diag-err:" + t.getClass().getName();
        }
    }

    public String getDisplayManagerDiag() {
        try {
            de.audi.atip.hmi.view.IDisplayManager dm =
                ((de.audi.atip.hmi.HMIService) this.env.getHMIService()).getDisplayManager();
            int ctx = -1;
            try {
                ctx = dm.getCurrentContextID(1);
            } catch (Throwable t) {
                /* ignore */
            }
            return "dmType=" + dm.getClass().getName()
                + " ctx1=" + ctx
                + " " + this.getDisplayManagerInitState(dm);
        } catch (Throwable t) {
            return "dmDiagFailed:" + t.getClass().getName() + ": " + t.getMessage();
        }
    }

    public String getFrameworkDiag() {
        try {
            IFrameworkAccess fw = this.env.getFramework();
            return "kombiType=" + fw.getKombiType()
                + " sys541=" + fw.getSysConst(541)
                + " sys4383=" + fw.getSysConst(4383)
                + " sys4388=" + fw.getSysConst(4388)
                + " mapFPK=" + Util.isClusterMapFPK(fw)
                + " mapMOST=" + Util.isClusterMapMOST(fw)
                + " setRouteInfoDSI=" + Util.isSetRouteInfoDSIAvailable(fw);
        } catch (Throwable t) {
            return "fwDiagFailed:" + t.getClass().getName() + ": " + t.getMessage();
        }
    }

    public String setClusterUpdateRateDiag(int rate) {
        try {
            de.audi.atip.hmi.view.IDisplayManager dm =
                ((de.audi.atip.hmi.HMIService) this.env.getHMIService()).getDisplayManager();
            int ctx = -1;
            try {
                ctx = dm.getCurrentContextID(1);
            } catch (Throwable t) {
                /* ignore */
            }
            String base = "rate=" + rate
                + " ctx1=" + ctx
                + " dmType=" + dm.getClass().getName()
                + " " + this.getDisplayManagerInitState(dm);
            dm.setUpdateRate(1, rate);
            return "OK " + base;
        } catch (Throwable t) {
            return "FAILED: " + t.getClass().getName() + ": " + t.getMessage();
        }
    }

    public void setClusterUpdateRate(int rate) {
        this.setClusterUpdateRateDiag(rate);
    }

    private String trySetVisibleKDKReflective(de.audi.atip.hmi.view.IDisplayManager dm, int displayable, int terminal) {
        try {
            java.lang.reflect.Method setMethod = dm.getClass()
                .getMethod("setKDKVisible", new Class[]{Integer.TYPE, Integer.TYPE});
            setMethod.invoke(dm, new Object[]{new Integer(displayable), new Integer(terminal)});
            try {
                java.lang.reflect.Method getMethod = dm.getClass().getMethod("getVisibleKDK", new Class[]{Integer.TYPE});
                Object result = getMethod.invoke(dm, new Object[]{new Integer(terminal)});
                if (result instanceof Integer) {
                    return String.valueOf(((Integer) result).intValue());
                }
            } catch (Throwable t) {
                return "set-only";
            }
            return String.valueOf(displayable);
        } catch (NoSuchMethodException e) {
            return "no-kdk-api";
        } catch (Throwable t) {
            return "kdk-err:" + t.getClass().getName();
        }
    }

    /**
     * Activate the cluster video pipeline directly on the DisplayManager.
     *
     * Native display manager calls setActiveDisplayable on videoencoderservice
     * ONLY from CContextManager::preContextSwitchHook during a REAL context
     * switch. Java DisplayManager.switchContext() skips the DSI call when the
     * requested context equals confirmedActiveContext -- so if context 72 is
     * already active, setActiveDisplayable never fires and videoencoderservice
     * captures displayable 0 (nothing).
     *
     * Context 72: displayables {33} -- base map only.
     * Context 74: displayables {20, 102, 101, 33} -- map + KDK widget.
     *   First displayable = 20 (KDK composited view with map underneath).
     *
     * The KOMO GuidanceView renders into displayable 20 (KDK area).
     * With PresentationController patches keeping the rendering pipeline alive
     * (NOP StopDSIs + force StartDrawing + hardcode 10fps), displayable 20
     * has content. We use context 74 so the encoder captures the composited
     * map+widget output.
     *
     * Steps:
     * 1. Ensure KDK mapping is set (context 72 -> 74).
     * 2. Force context switch: 0 -> 72 (triggers preContextSwitchHook).
     *    With KDK mapping active, context 72 becomes 74 internally ->
     *    setActiveDisplayable(4, 20) -> encoder captures composited view.
     * 3. Start video encoding at 10fps.
     */
    public String activateClusterVideoPipeline() {
        try {
            de.audi.atip.hmi.view.IDisplayManager dm =
                ((de.audi.atip.hmi.HMIService) this.env.getHMIService()).getDisplayManager();
            int ctxBefore = dm.getCurrentContextID(1);

            /* 1. Set KDK mapping: context 72 -> 74 (includes displayable 20).
             *    setKDKVisible(20, terminal) adds KDK displayable overlay. */
            String kdkSet = this.trySetVisibleKDKReflective(dm, 20, 1);
            try { Thread.sleep(200); } catch (InterruptedException ie) { /* ignore */ }

            /* 2. Force context away so next switchContext is a real change. */
            dm.switchContext(0, 1, null);
            int ctxAfterReset = dm.getCurrentContextID(1);

            /* 3. Wait for native DM to confirm the context switch via DSI. */
            try { Thread.sleep(300); } catch (InterruptedException ie) { /* ignore */ }

            /* 4. Switch to FPK context 72. With KDK mapping active, this
             *    becomes context 74 internally -> preContextSwitchHook ->
             *    setActiveDisplayable(4, 20) -> encoder captures KDK composite
             *    (map + widget). */
            dm.switchContext(72, 1, null);
            int ctxAfterMap = dm.getCurrentContextID(1);

            try { Thread.sleep(300); } catch (InterruptedException ie) { /* ignore */ }

            /* 5. Start video encoding at 10fps. */
            dm.setUpdateRate(1, 10);

            return "ctx=" + ctxBefore + "->" + ctxAfterReset + "->" + ctxAfterMap
                + " kdk=" + kdkSet
                + " dmType=" + dm.getClass().getName()
                + " " + this.getDisplayManagerInitState(dm);
        } catch (Throwable t) {
            return "FAILED: " + t.getClass().getName() + ": " + t.getMessage();
        }
    }

    public void deactivateClusterVideoPipeline() {
        try {
            de.audi.atip.hmi.view.IDisplayManager dm =
                ((de.audi.atip.hmi.HMIService) this.env.getHMIService()).getDisplayManager();
            dm.setUpdateRate(1, 0);
        } catch (Throwable t) {
            /* non-fatal */
        }
    }

    /**
     * Update followInfoRIE trip data fields so PresentationController sees
     * changed data on each setRouteInfo() call and re-renders the arrow.
     * Also clears empty-string defaults from the widget.
     */
    public void updateFollowInfoData(String distance, String eta, String turnTo) {
        if (this.followInfoRIE != null) {
            this.followInfoRIE.distanceToElement = (distance != null) ? distance : "";
            this.followInfoRIE.estimatedTimeToElement = (eta != null) ? eta : "";
            this.followInfoRIE.turnToStreet = turnTo;
            if (this.followInfoRIE.trafficInfo != null) {
                this.followInfoRIE.trafficInfo.trafficOffset = "";
                this.followInfoRIE.trafficInfo.trafficOffsetAffix = "";
            }
        }
        this.env.getLabelModel(71).setText((turnTo != null) ? turnTo : "");
    }

    /**
     * Activate custom renderer video pipeline.
     * Ensures the cluster is on context 74.  The renderer process has already
     * taken over native displayable 20 (DISPLAYABLE_MAP_ROUTE_GUIDANCE) by
     * registering its own screen window with ID="20" in displaymanager's
     * m_surfaceSources; we just need the cluster on context 74 so the MOST
     * encoder's setActiveDisplayable(4, 20) hook reads from our window.
     */
    public String activateCustomRendererPipeline() {
        /* Renderer takes over native displayable 20 (KOMO RG widget slot).
         * Re-issue the context switch defensively because native navigation
         * or another HMI process can leave the cluster on a different context. */
        try {
            de.audi.atip.hmi.view.IDisplayManager dm =
                ((de.audi.atip.hmi.HMIService) this.env.getHMIService()).getDisplayManager();
            int ctxBefore = dm.getCurrentContextID(1);
            /* Ensure cluster is on context 74 (with map + RG widget) */
            if (ctxBefore != 74) {
                dm.switchContext(74, 1, null);
                try { Thread.sleep(150); } catch (InterruptedException ie) { /* ignore */ }
            }
            int ctxAfter = dm.getCurrentContextID(1);
            if (ctxAfter != 74) {
                dm.switchContext(74, 1, null);
                try { Thread.sleep(150); } catch (InterruptedException ie) { /* ignore */ }
                ctxAfter = dm.getCurrentContextID(1);
            }
            if (ctxAfter != 74) {
                return "FAILED: cluster ctx=" + ctxBefore + "->" + ctxAfter + " not 74";
            }
            return "cluster ctx=" + ctxBefore + "->" + ctxAfter;
        } catch (Throwable t) {
            return "FAILED: " + t.getClass().getName() + ": " + t.getMessage();
        }
    }

    /**
     * Deactivate custom renderer pipeline. Stop encoding and restore context.
     */
    public void deactivateCustomRendererPipeline() {
        /* Backstop for renderer teardown: the renderer normally restores
         * context 74 from its own atexit handler, but Java may have to slay
         * the process if TCP teardown races.  Re-declare the native context
         * here so the cluster compositor doesn't keep our (now destroyed)
         * screen window mapped to displayable 20. */
        try {
            de.audi.atip.util.CommandLineExecuter.executeCommand(
                "/bin/sh", new String[] { "-c", "/eso/bin/apps/dmdt dc 74 20 102 101 33 >/dev/null 2>&1" });
        } catch (Throwable t) {
            /* non-fatal */
        }

        try {
            de.audi.atip.hmi.view.IDisplayManager dm =
                ((de.audi.atip.hmi.HMIService) this.env.getHMIService()).getDisplayManager();
            dm.switchContext(74, 1, null);
        } catch (Throwable t) {
            /* non-fatal */
        }
    }

}
