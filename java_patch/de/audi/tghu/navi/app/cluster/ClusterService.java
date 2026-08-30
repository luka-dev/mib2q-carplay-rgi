package de.audi.tghu.navi.app.cluster;

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
import de.audi.tghu.navi.app.command.DSIResponseContainer;
import de.audi.tghu.navi.app.NavigationEnv;
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
import de.audi.tghu.navi.app.rp.TripHandler;
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

    /* HMI model ids driven on the cluster (env.getMetricsModel/getChoiceModel).  Only the ids whose
     * meaning is unambiguous from usage are named; 62/69/71/162 are left as literals on purpose. */
    private static final int MODEL_ARRIVAL_TIME        = 66;   // ETA date metric (travel params)
    private static final int MODEL_DIST_TO_DESTINATION = 63;   // remaining distance metric
    private static final int MODEL_DIST_TO_MANEUVER    = 64;   // distance-to-next-maneuver (FctID 18)
    private static final int MODEL_BARGRAPH            = 65;   // approach bargraph choice (FctID 18)

    /* MetricsModel status (Util.setModelStatus/setStatus): 1 = valid/shown, 3 = invalid/hidden. */
    private static final int MODEL_STATUS_VALID   = 1;
    private static final int MODEL_STATUS_INVALID = 3;

    /* Cluster MOST/LVDS display contexts (switchDisplayContextKombi - NOT DisplayManager contexts). */
    private static final int KOMBI_CTX_MAP      = 8;   // map available
    private static final int KOMBI_CTX_KDK_ONLY = 9;   // KDK-only cluster
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
        NavigationEnv navigationenv,
        SpeechManager speechmanager,
        OperationManager operationmanager,
        AudioStateMachine audiostatemachine,
        MapManager mapmanager,
        ICommandListFactory icommandlistfactory
    ) {
        this(
            navigationenv,
            speechmanager,
            operationmanager,
            audiostatemachine,
            mapmanager,
            icommandlistfactory,
            new NullViewSizeManager(navigationenv.getClusterLogChannel()),
            new NullViewSizeChangeHandler()
        );
    }

    protected ClusterKDKHandler initClusterKDKHandler(IViewSizeChangeHandler iviewsizechangehandler) {
        return new ClusterKDKHandlerImpl(this.env, iviewsizechangehandler, this.combiBAPListener);
    }

    public ClusterService(
        NavigationEnv navigationenv,
        SpeechManager speechmanager,
        OperationManager operationmanager,
        AudioStateMachine audiostatemachine,
        MapManager mapmanager,
        ICommandListFactory icommandlistfactory,
        IViewSizeManager iviewsizemanager,
        IViewSizeChangeHandler iviewsizechangehandler
    ) {
        this.env = navigationenv;
        this.mapInterface = mapmanager.getMapInterface();
        this.logChannel = navigationenv.getClusterLogChannel();
        this.bapDistanceFormatter = new BAPDistanceFormatter(this.logChannel);
        this.travelParametersGroup = new ModelGroup();
        this.nextManeuverGroup = new ModelGroup();
        this.etaDateMetric = new DateMetric(new Date(), 1);
        this.rttDateMetric = new DateMetric(new Date(), 3);
        this.followInfoRIE = new RouteInfoElement(
            null, null, 0, null, null, null, 0, null, null, null, null, new TrafficInfo(), 0, null, null, null, null, 0
        );
        this.combiBAPListener = this.initBAPListener(
            speechmanager, operationmanager, audiostatemachine, mapmanager, icommandlistfactory, iviewsizemanager
        );
        this.clusterViewMode = new ClusterViewMode(navigationenv, this);
        this.clusterKDKHandler = this.initClusterKDKHandler(iviewsizechangehandler);
        this.komoService = new KOMOService(navigationenv, this, this.clusterKDKHandler);
        this.clusterInputListener = this.createClusterInputListener(navigationenv);
        this.mapScaleHandler = new MapScaleHandler();
        this.mapScaleTimer = new MapScaleTimer(navigationenv, this, this.mapScaleHandler);
        navigationenv.getLabelModel(62).setText("");
        this.initializeModels();
    }

    private void initializeModels() {
        this.logChannel.log(10000000, "ClusterService#initializeModels() ");
        this.env.getLabelModel(71).setText("");
        this.env.getChoiceModel(69).setValue(0);
        this.turnToStreetValid = false;
        this.turnToStreet = "";
        this.env.getMetricsModel(MODEL_ARRIVAL_TIME).setStatus(MODEL_STATUS_INVALID);
        this.env.getMetricsModel(MODEL_ARRIVAL_TIME).setMetric(this.etaDateMetric);
        this.env.getMetricsModel(MODEL_DIST_TO_DESTINATION).setStatus(MODEL_STATUS_INVALID);
        this.env.getMetricsModel(MODEL_DIST_TO_DESTINATION).setMetric(this.distanceToDestination);
        this.travelParametersGroup.add(this.env.getMetricsModel(MODEL_ARRIVAL_TIME));
        this.travelParametersGroup.add(this.env.getMetricsModel(MODEL_DIST_TO_DESTINATION));
        this.env.getMetricsModel(MODEL_DIST_TO_MANEUVER).setStatus(MODEL_STATUS_INVALID);
        this.env.getMetricsModel(MODEL_DIST_TO_MANEUVER).setMetric(this.distanceToManeuver);
        this.env.getChoiceModel(MODEL_BARGRAPH).setValue(-1);
        this.showBargraph = false;
        this.nextManeuverGroup.add(this.env.getMetricsModel(MODEL_DIST_TO_MANEUVER));
        this.nextManeuverGroup.add(this.env.getChoiceModel(MODEL_BARGRAPH));
        if (Util.isClusterKDKOnly(this.env.getFramework())) {
            this.switchDisplayContextKombi(KOMBI_CTX_KDK_ONLY);
        } else if (Util.isClusterMapAvailable(this.env.getFramework())) {
            this.switchDisplayContextKombi(KOMBI_CTX_MAP);
        }
    }

    protected CombiBAPListener initBAPListener(
        SpeechManager speechmanager,
        OperationManager operationmanager,
        AudioStateMachine audiostatemachine,
        MapManager mapmanager,
        ICommandListFactory icommandlistfactory,
        IViewSizeManager iviewsizemanager
    ) {
        return new ScreenCombiBAPListener(
            this,
            this.logChannel,
            this.env,
            speechmanager,
            operationmanager,
            audiostatemachine,
            mapmanager,
            icommandlistfactory,
            iviewsizemanager
        );
    }

    public synchronized void unitsChanged(TripHandler.TripData triphandler$tripdata) {
        this.logChannel.log(10000000, "ClusterService#unitsChanged()");
        this.refreshTravelParameters(triphandler$tripdata);
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

    protected ClusterInputListener createClusterInputListener(NavigationEnv navigationenv) {
        return new ClusterInputListener(navigationenv, this);
    }

    public CombiBAPServiceNaviListener getCombiBAPListener() {
        return this.combiBAPListener;
    }

    public void setCombiBAPService(CombiBAPServiceNavi combibapservicenavi) {
        this.logChannel.log(10000000, "ClusterService#setCombiBAPService()");
        /* This setter runs on NavigationJobs.  Install the transparent CarPlay gate BEFORE the
         * stock setter's immediate updateAll(), so native NavStatus never paints across the
         * takeover edge and no other dispatcher writes CombiBAPListener.combiservice. */
        CombiBAPServiceNavi effective = com.luka.carplay.core.ScreenNavStatusGate
            .prepareCombiBAPService(this, combibapservicenavi);
        this.combiBAPListener.setCombiService(effective);
        this.combiBAPServiceNavi = combibapservicenavi;
        /* CarPlay's map plane may already be live during a cold boot.  Notify the optional
         * NavStatus wrapper on the exact OSGi service edge instead of making the cluster module poll -- or
         * worse, wait for stock Navigation before showing its own video. */
        com.luka.carplay.core.ScreenNavStatusGate.onCombiBAPServiceChanged(this);
    }

    public void setCombiService(CombiService combiservice) {
        this.logChannel.log(10000000, "ClusterService#setCombiService()");
        this.clusterViewMode.setCombiService(combiservice);
        this.combiDDP2ServiceNavi = combiservice;
    }

    public void setMOSTFrameVisible(boolean flag) {
        this.logChannel.log(10000000, "ClusterService#setMOSTFrameVisible( %1 )", flag);
        this.komoService.notifyVisibility(flag);
    }

    public boolean isLvdsMapVisible() {
        return this.combiBAPListener.lvdsMapVisible;
    }

    public void updateCurrentStreet(String s) {
        this.logChannel.log(100000000, "ClusterService#updateCurrentStreet( %1 )", s);
        String s1 = s;
        String s2 = s;
        String s3 = s;
        if (Util.isEmpty(s1)) {
            this.logChannel.log(10000000, "ClusterService#updateCurrentStreet() - invalid currentStreet: %1", s1);
            s2 = "";
            s1 = "";
            s3 = EMPTY_STREET_LABEL;
        }

        this.env.getLabelModel(62).setText(s3);
        this.komoService.setCurrentStreet(s1);
        this.combiBAPListener.setCurrentStreet(s2);
    }

    public void updateTurnToStreet(String s, boolean flag) {
        this.logChannel.log(100000000, "ClusterService#updateTurnToStreet( %1 )", s);
        this.turnToStreet = s;
        this.turnToStreetValid = !Util.isEmpty(s);
        this.env.getLabelModel(71).setText(s);
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
        ChoiceModelApp choicemodelapp = this.env.getChoiceModel(69);
        if (this.turnToStreetValid && this.showBargraph) {
            choicemodelapp.setValue(1);
            this.komoService
                .setTurnToStreet(this.turnToStreet, "");
        } else {
            choicemodelapp.setValue(0);
            this.komoService
                .setTurnToStreet(
                    "",
                    ""
                );
        }
    }

    public void refreshDistanceToNextManeuver() {
        DistanceToNextManeuver distancetonextmaneuver = this.env.getContainer().getDistanceToNextManeuver();
        this.refreshDistanceToNextManeuver(distancetonextmaneuver);
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
                    .log(100000, "ClusterService#convertBAP2KOMOUnit() - unknown or not convertable BAP unit: %1", i);
                return 255;
        }
    }

    protected void refreshDistanceToNextManeuver(DistanceToNextManeuver distancetonextmaneuver) {
        if (distancetonextmaneuver == null) {
            this.logChannel
                .log(1000000, "ClusterService#refreshDistanceToNextManeuver() - distanceToNextManeuver is null! ");
        } else {
            this.logChannel
                .log(
                    100000000,
                    "ClusterService#refreshDistanceToNextManeuver() - distanceToNextManeuver: %1",
                    distancetonextmaneuver
                );
            boolean flag = distancetonextmaneuver.showDistance;
            int i = distancetonextmaneuver.distance;
            this.showBargraph = distancetonextmaneuver.showBargraph;
            int j = distancetonextmaneuver.bargraph;
            long k;
            int l;
            boolean flag1;
            if (i > 0) {
                BAPDistanceFormatter.BAPDistance bapdistanceformatter$bapdistance = this.bapDistanceFormatter
                    .formatDistanceToTurn(i, Distance.getSystemUnit() == 1);
                k = bapdistanceformatter$bapdistance.getValue();
                l = this.convertBAP2KOMODistanceUnit(bapdistanceformatter$bapdistance.getUnit());
                flag1 = !this.showBargraph;
            } else {
                k = -1L;
                l = 255;
                flag1 = false;
            }

            MetricsModelApp metricsmodelapp = this.env.getMetricsModel(MODEL_DIST_TO_MANEUVER);
            if (flag && i > 0) {
                this.distanceToManeuver.setValue(i / 1000.0F);
                metricsmodelapp.setMetric(this.distanceToManeuver);
            }

            /* FctID 18: distance number and bargraph are INDEPENDENT records in the BAP spec and
             * must be able to render together.  Stock gated the distance-valid status on
             * !showBargraph, making them mutually exclusive (distance vanished whenever the
             * bargraph was active).  Drop the !showBargraph term so the number stays valid
             * alongside the bargraph.  (Model 65 bargraph value is set independently below.) */
            if (flag && i > 0 && (!this.showBargraph || com.luka.carplay.core.ScreenModule.isConnected())) {
                Util.setModelStatus(metricsmodelapp, MODEL_STATUS_VALID);
            } else {
                Util.setModelStatus(metricsmodelapp, MODEL_STATUS_INVALID);
            }

            int i1 = this.showBargraph ? j : -1;
            this.env.getChoiceModel(MODEL_BARGRAPH).setValue(i1);
            this.refreshStreetMode();
            this.nextManeuverGroup.flush();
            this.refreshDistanceToNextManeuverMOST(k, l, flag1, flag);
            this.combiBAPListener.setDistanceToNextManeuver(i, this.showBargraph, j);
        }
    }

    public void refreshTravelParameters(TripHandler.TripData triphandler$tripdata) {
        this.logChannel.log(100000000, "ClusterService#refreshTravelParameters()");
        if (triphandler$tripdata.etaModeActive) {
            this.updateArrivalTime(
                triphandler$tripdata.etaValid,
                triphandler$tripdata.etaToNextDestination,
                triphandler$tripdata.isTimeZoneOffset
            );
        } else {
            this.updateRemainingTravelTime(
                triphandler$tripdata.etaValid, triphandler$tripdata.timeToNextDestination * 1000L
            );
        }

        this.followInfoRIE.destinationIndex = this.getDestIndex();
        this.updateDistanceToDestination(
            triphandler$tripdata.distanceToNextDestination, this.followInfoRIE.destinationIndex == 0
        );
        this.updateKOMOFollowInfo();
        this.travelParametersGroup.flush();
    }

    private void clearRouteInfoElement(RouteInfoElement routeinfoelement) {
        if (routeinfoelement != null) {
            routeinfoelement.distanceToElement = "";
            routeinfoelement.estimatedTimeToElement = "--:--";
            routeinfoelement.routeInfoElementType = 0;
            routeinfoelement.elementIconIDs = null;
            routeinfoelement.prio1EventText = null;
            routeinfoelement.streetIconText = null;
            routeinfoelement.streetIconID = 0;
            routeinfoelement.exitNumber = null;
            routeinfoelement.turnToStreet = null;
            routeinfoelement.pOIElementNames = null;
            routeinfoelement.maneuverDescriptor = null;
            if (routeinfoelement.trafficInfo != null) {
                routeinfoelement.trafficInfo.trafficOffset = null;
                routeinfoelement.trafficInfo.trafficOffsetAffix = null;
                routeinfoelement.trafficInfo.affixPlacementBefore = false;
            }

            routeinfoelement.destinationIndex = 0;
            routeinfoelement.signPostInfo = null;
            routeinfoelement.distanceToManeuver = null;
            routeinfoelement.estimatedTimeToManeuver = null;
            routeinfoelement.streetCardinalDirection = null;
            routeinfoelement.exitIconId = 0;
        }
    }

    protected void refreshDistanceToNextManeuverMOST(long i, int j, boolean flag, boolean flag1) {
        this.komoService.setDistanceToNextManeuver(i, j, flag);
    }

    protected int getDestIndex() {
        int i = this.mapInterface.getMap().getNaviInterface().getRouteListLength();
        int j = this.mapInterface.getMap().getNaviInterface().getIndexOfCurrentDestination();
        return j + 1 < i ? 0 : -1;
    }

    protected void updateArrivalTime(boolean flag, long i, boolean flag1) {
        this.logChannel.log(100000000, "ClusterService#updateArrivalTime( %1, %2 )", flag, i);
        MetricsModelApp metricsmodelapp = this.env.getMetricsModel(MODEL_ARRIVAL_TIME);
        String s;
        if (flag) {
            this.etaDateMetric.setDate(i);
            metricsmodelapp.setMetric(this.etaDateMetric);
            Util.setModelStatus(metricsmodelapp, MODEL_STATUS_VALID);
            s = Util.formatTime(i, 2, this.env);
        } else {
            this.logChannel.log(10000000, "ClusterService#updateArrivalTime() - invalid flag for ETA set! ");
            Util.setModelStatus(metricsmodelapp, MODEL_STATUS_INVALID);
            s = "--:--";
        }

        int j = KOMOService.convertTimeFormatToKOMO(DateMetric.timeFormat);
        KOMOTime komotime = KOMOService.convertTimeToKOMO(i);
        this.followInfoRIE.estimatedTimeToElement = s;
        this.komoService.setETA(j, komotime.day, komotime.hour, komotime.min, flag, flag1);
        this.combiBAPListener.setRgTimeToNextDestination(i, true, flag);
    }

    protected void updateRemainingTravelTime(boolean flag, long i) {
        this.logChannel.log(100000000, "ClusterService#RemainingTravelTime( %1, %2 )", flag, i);
        MetricsModelApp metricsmodelapp = this.env.getMetricsModel(MODEL_ARRIVAL_TIME);
        if (flag) {
            this.rttDateMetric.setDate(i);
            metricsmodelapp.setMetric(this.rttDateMetric);
            Util.setModelStatus(metricsmodelapp, MODEL_STATUS_VALID);
        } else {
            this.logChannel.log(10000000, "ClusterService#updateArrivalTime() - invalid flag for ETA set! ");
            Util.setModelStatus(metricsmodelapp, MODEL_STATUS_INVALID);
        }

        KOMOTime komotime = KOMOService.convertDurationToKOMO(i);
        this.komoService.setRTT(komotime.hour, komotime.min, flag);
        this.combiBAPListener.setRgTimeToNextDestination(i, false, flag);
    }

    protected void updateDistanceToDestination(int i, boolean flag) {
        this.logChannel.log(100000000, "ClusterService#updateDistanceToDestination( %1 )", i);
        long j = -1L;
        int k = -1;
        MetricsModelApp metricsmodelapp = this.env.getMetricsModel(MODEL_DIST_TO_DESTINATION);
        String s;
        boolean flag1;
        if (i > 0) {
            this.distanceToDestination.setValue(i / 1000.0F);
            metricsmodelapp.setMetric(this.distanceToDestination);
            Util.setModelStatus(metricsmodelapp, MODEL_STATUS_VALID);
            s = Util.formatDistance(i, 1, 2, EMPTY_STREET_LABEL);
            BAPDistanceFormatter.BAPDistance bapdistanceformatter$bapdistance = this.bapDistanceFormatter
                .formatDistanceToDestination(i, Distance.getSystemUnit() == 1);
            j = bapdistanceformatter$bapdistance.getValue();
            k = this.convertBAP2KOMODistanceUnit(bapdistanceformatter$bapdistance.getUnit());
            flag1 = true;
        } else {
            this.logChannel
                .log(10000000, "ClusterService#updateDistanceToDestination() - invalid distanceToDestination! ");
            Util.setModelStatus(metricsmodelapp, MODEL_STATUS_INVALID);
            s = "";
            flag1 = false;
        }

        this.followInfoRIE.distanceToElement = s;
        this.komoService.setDistanceToDestination(j, k, flag1);
        this.combiBAPListener.setRgDistanceToNextDestination(i, flag);
    }

    public void updateSoPosPosition(PosPosition posposition) {
        this.logChannel.log(100000000, "ClusterService#updateSoPosPosition( %1 )", posposition);
        short short1 = 0;
        short short2 = 255;
        int i = -1;
        if (posposition != null) {
            short1 = (short)posposition.getDirectionAngle();
            short2 = (short)posposition.getDirectionSymbolic();
            i = posposition.getState();
        } else {
            this.logChannel.log(100000, "ClusterService#updateSoPosPosition() - invalid soPosPosition!");
        }

        this.combiBAPListener.setVehicleHeading(short1, short2);
        this.combiBAPListener.setInfoStateGPS(i);
    }

    public void updateSoPosPositionDescription(NavLocation navlocation) {
        this.logChannel.log(100000000, "ClusterService#updateSoPosPositionDescription( %1 )", navlocation);
        String s = LocationFormatter.formatCity(navlocation);
        this.komoService.setCityName(s);
    }

    public void updateRgDirectionToNextDestination(short short1) {
        this.logChannel.log(100000000, "ClusterService#updateRgDirectionToNextDestination( %1 )", short1);
    }

    public void updateRGIString(short[] ashort) {
        if (ashort != null && ashort.length > 0) {
            this.rgiDataValid = true;
            this.env.getDataModel(68).set(ashort);
        } else {
            this.rgiDataValid = false;
            this.logChannel.log(10000000, "ClusterService#updateRGIData() - invalid RGI data ");
        }

        this.refreshRGIValid();
    }

    public void updateRgActive(boolean flag) {
        this.logChannel.log(100000000, "ClusterService#updateRgActive( %1 )", flag);
        this.refreshRGIValid();
        this.clusterViewMode.refreshRGState();
        if (!flag) {
            this.initializeModels();
            this.updateTurnToStreet("", false);
            this.clearRouteInfoElement(this.followInfoRIE);
            this.clearRouteInfoElement(this.nextManeuverElement);
            this.updateDestinationInfo(null, 0, 0);
        }

        this.clusterKDKHandler.updateRgActive(flag);
        this.combiBAPListener.setRgActive(flag);
    }

    public void updateNavState(int i, int j) {
        this.logChannel.log(10000000, "ClusterService#updateNavState( %1, %2 )", i, j);
        if (this.combiDDP2ServiceNavi != null) {
            this.combiDDP2ServiceNavi.updateNavInitialized(i, j);
        }
    }

    private void refreshRGIValid() {
        boolean flag = this.env.getContainer().isRgActive();
        boolean flag1 = flag && this.rgiDataValid;
        this.logChannel
            .log(
                100000000, "ClusterService#refreshRGIValid() - rgActive: %1, rgiDataValid: %2", flag, this.rgiDataValid
            );
        this.clusterViewMode.setRGIValid(flag1);
    }

    public void refreshViewMode(int i) {
        this.combiBAPListener.setViewMode(i);
    }

    public void updateLaneGuidance(NavLaneGuidanceData[] anavlaneguidancedata, boolean flag) {
        this.combiBAPListener.setLaneGuidance(anavlaneguidancedata, flag);
    }

    public void updateManeuverDescriptor(BapManeuverDescriptor[] abapmaneuverdescriptor) {
        this.rgiDataValid = abapmaneuverdescriptor != null && abapmaneuverdescriptor.length > 0;
        this.refreshRGIValid();
        this.combiBAPListener.setManeuverDescriptor(abapmaneuverdescriptor);
    }

    public void updateManeuverDescriptor(BapManeuverDescriptor[] abapmaneuverdescriptor, int i) {
        this.rgiDataValid = abapmaneuverdescriptor != null && abapmaneuverdescriptor.length > 0;
        this.refreshRGIValid();
        this.combiBAPListener.setManeuverDescriptor(abapmaneuverdescriptor, i);
    }

    public String toString() {
        Buffer buffer = new Buffer();
        buffer.append(this.komoService);
        buffer.append(this.clusterViewMode);
        return buffer.toString();
    }

    public void updateBapTurnToInfo(BapTurnToInfo[] abapturntoinfo) {
        this.combiBAPListener.setTurnToInfo(abapturntoinfo);
    }

    public void updateInfoStatesGPS(int i) {
        this.combiBAPListener.setInfoStateGPS(i);
    }

    private boolean initScreenNeededOnKombi() {
        boolean flag = this.env.getContainer().getNavstateOfOperation() == 5;
        boolean flag1 = Util.isClusterMapAvailable(this.env.getFramework());
        boolean flag2 = false;
        AbstractMap abstractmap = this.mapInterface.getKombiMap();
        if (abstractmap != null && abstractmap.isInitialized()) {
            flag2 = true;
        }

        return flag && !flag2 && flag1 ? true : !this.operationStateIsKnownToTheKombi;
    }

    public synchronized void updateOperationState(int i) {
        this.logChannel.log(1000000, "ClusterService#updateOperationState( %1 )", i);
        this.operationStateIsKnownToTheKombi = this.combiBAPListener.setInfoStateNavi(i);
        this.combiBAPListener.forceShowInitScreen(this.initScreenNeededOnKombi());
        if (this.combiBAPListener.lvdsMapVisible && this.env.getContainer().getNavstateOfOperation() == 5) {
            this.showKombiMap(true);
        }
    }

    public void updateXUrgentMessages(TmcMessage[] atmcmessage) {
        this.combiBAPListener.setXUrgentMessages(atmcmessage);
    }

    public void updateMessagesOnRoute(TmcMessage[] atmcmessage) {
        this.combiBAPListener.setMessagesOnRoute(atmcmessage);
    }

    public void setRouteGuidanceAborted() {
        this.combiBAPListener.setRouteGuidanceAborted();
    }

    public void updateRgInfoForNextDestination(RgInfoForNextDestination rginfofornextdestination) {
        this.logChannel.log(100000000, "ClusterService#updateRgInfoForNextDestination(%1)", rginfofornextdestination);
        this.rgInfoForNextDestination = rginfofornextdestination;
        this.updateRgDirectionToNextDestination(rginfofornextdestination.getDirectionToNextDest());
    }

    public void updateDistanceToNextManeuver(DistanceToNextManeuver distancetonextmaneuver) {
        this.refreshDistanceToNextManeuver(distancetonextmaneuver);
    }

    public synchronized void updateKombiMapReady(boolean flag) {
        this.logChannel.log(1000000, "ClusterService#updateKombiMapReady( %1 )", flag);
        if (flag) {
            this.switchDisplayContextKombi(KOMBI_CTX_MAP);
            if (Util.isClusterMapAlwaysOn()) {
                this.combiBAPListener.setMainMapVisibility(true);
            }

            if (this.combiBAPListener.lvdsMapVisible) {
                this.showKombiMap(true);
            }
        }

        this.combiBAPListener.forceShowInitScreen(this.initScreenNeededOnKombi());
        this.getClusterViewMode().setKombiMapReady(flag);
    }

    public void showKombiMap(boolean flag) {
        this.logChannel.log(1000000, "ClusterService#showKombiMap( %1 )", flag);
        this.mapInterface.showKombiMap(flag);
    }

    public void setSupplementaryMap(int i, boolean flag) {
        this.logChannel
            .log(10000000, "ClusterService#setSupplementaryMap() - supplementaryMapView: %2, visible: %1", flag, i);
        if (i != 1 && flag) {
            this.logChannel
                .log(
                    100000,
                    "ClusterService#setSupplementaryMap() - got request to show supplementary map although not available or unimplemented"
                );
        }

        this.setKDKVisibility(flag);
    }

    public void switchDisplayContextKombi(int i) {
        this.logChannel.log(1000000, "ClusterService#switchDisplayContextKombi( %1 )", i);
        this.mapInterface.switchDisplayContextKombi(i);
    }

    public void setKOMODataRate(int i) {
        if (Util.isClusterMapMOST(this.env.getFramework())) {
            this.setKOMODataRate(i, true);
        }
    }

    public int getKOMODataRate() {
        this.logChannel.log(100000000, "ClusterService#getKOMODataRate()");
        ChoiceModelApp choicemodelapp = this.env.getChoiceModel(1, 168);
        int i = choicemodelapp.getHints();
        boolean flag = (i & 2) == 2;
        boolean flag1 = (i & 1) == 1;
        if (flag) {
            if (flag1) {
                this.logChannel
                    .log(
                        100000,
                        "ClusterService#getKOMODataRate() - undefined state: full and reduced framerate -> assuming full framerate"
                    );
            }

            return 2;
        } else {
            return flag1 ? 1 : 0;
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

    private void setKOMODataRate(int i, boolean flag) {
        this.logChannel.log(1000000, "ClusterService#setKOMODataRate( %1 )", i);
        synchronized (this.komoDataRateMutex) {
            ChoiceModelApp choicemodelapp = this.env.getChoiceModel(1, 168);
            if (i == 2) {
                choicemodelapp.removeHint(1);
                choicemodelapp.addHint(2);
            } else if (i == 1) {
                choicemodelapp.removeHint(2);
                choicemodelapp.addHint(1);
            } else {
                choicemodelapp.removeHint(2);
                choicemodelapp.removeHint(1);
            }

            if (flag) {
                choicemodelapp.publishHints();
            }
        }
    }

    public void onAutoZoomStateChanged(boolean flag) {
        this.combiBAPListener.setAutoZoomActive(flag);
    }

    public void updateKOMOFollowInfo() {
        this.logChannel.log(100000000, "ClusterService#updateKOMOFollowInfo())");
        if (this.komoService != null && Util.isKOMOFollowMode(this.env.getFramework())) {
            try {
                this.logChannel
                    .log(
                        100000000,
                        "ClusterService#updateKOMOFollowInfo() - followInfoRIE: %1, nextManeuverElement: %2",
                        this.followInfoRIE,
                        this.nextManeuverElement
                    );
                if (Util.isSetRouteInfoDSIAvailable(this.env.getFramework())) {
                    this.komoService.setRouteInfo(new RouteInfoElement[]{this.followInfoRIE, this.nextManeuverElement});
                } else {
                    this.komoService.setRouteInfoElement(this.followInfoRIE);
                }
            } catch (Exception exception) {
                this.logChannel.log(10000, "ClusterService#updateFollowInfo() - ERROR=%1", exception);
            }
        }
    }

    public void updateAltitude(int i) {
        this.logChannel.log(100000000, "ClusterService#updateAltitude( %1 )", i);
        this.combiBAPListener.setAltitude(i);
    }

    private String getDestinationDescription4BAP(NavLocation navlocation) {
        if (navlocation != null && navlocation.isPositionValid()) {
            try {
                LocationFormattingResponse locationformattingresponse = AddressFormatter.formatTwoLines(
                    navlocation, this.env
                );
                String s = locationformattingresponse.getFirstLineAsText();
                return s != null ? s : "";
            } catch (Exception exception) {
                this.env
                    .getLogChannel()
                    .log(
                        100000,
                        "Util#getString4BAPFromNavLocation - got an exception from AddressFormatter#formatTwoLines: %1",
                        exception
                    );
                return "";
            }
        } else {
            return "";
        }
    }

    private CombiBAPNaviDestination getBAPNaviDestFromLocation(NavLocation navlocation) {
        if (navlocation == null) {
            return new CombiBAPNaviDestination();
        }

        IMyLocationAccessor imylocationaccessor = Util.getLocationAccessor(navlocation);
        String s = this.getDestinationDescription4BAP(navlocation);
        if (Util.isEmpty(s)) {
            s = Util.isEmpty(imylocationaccessor.getPoiName()) ? null : imylocationaccessor.getPoiName();
        }

        return new CombiBAPNaviDestination(
            null,
            null,
            Util.isEmpty(navlocation.getStreet()) ? null : navlocation.getStreet(),
            Util.isEmpty(navlocation.getHousenumber()) ? null : navlocation.getHousenumber(),
            Util.isEmpty(navlocation.getTown()) ? null : navlocation.getTown(),
            Util.isEmpty(navlocation.getTownRefinement()) ? null : navlocation.getTownRefinement(),
            Util.isEmpty(imylocationaccessor.getState()) ? null : imylocationaccessor.getState(),
            Util.isEmpty(navlocation.getZipCode()) ? null : navlocation.getZipCode(),
            Util.isEmpty(navlocation.getCountry()) ? null : navlocation.getCountry(),
            wgs84ToDeg(navlocation.getLatitude()),
            wgs84ToDeg(navlocation.getLongitude()),
            imylocationaccessor.getType() == 1 ? 255 : 0,
            s,
            Util.isEmpty(imylocationaccessor.getPoiCategory()) ? null : imylocationaccessor.getPoiCategory(),
            255
        );
    }

    public void updateDestinationInfo(NavLocation navlocation, int i, int j) {
        if (this.logChannel.isDebug2()) {
            this.logChannel
                .log(
                    100000000,
                    "ClusterService#updateDestinationInfo() - noOfStopovers: %2, noOfNextStopover: %3, nextDestination: %1",
                    LocationFormatter.formatLocationShort(navlocation),
                    i,
                    j
                );
        }

        CombiBAPNaviDestination combibapnavidestination = this.getBAPNaviDestFromLocation(navlocation);
        CombiBAPDestinationInfo combibapdestinationinfo;
        if (navlocation != null) {
            combibapdestinationinfo = new CombiBAPDestinationInfo(combibapnavidestination);
            combibapdestinationinfo.setStopoverInformation(i, j);
        } else {
            combibapdestinationinfo = new CombiBAPDestinationInfo(combibapnavidestination);
            combibapdestinationinfo.setStopoverInformation(0, 0);
        }

        this.combiBAPListener.setDestinationInfo(combibapdestinationinfo);
    }

    public void updateSemidynamicRouteGuidance(RcciEvent rccievent) {
        this.logChannel
            .log(
                10000000,
                "ClusterService#updateSemidynamicRouteGuidance() - TrafficImpactOnCurrentRoute: %1, delay: %2",
                rccievent.hasTrafficImpactOnCurrentRoute(),
                rccievent.delay
            );
        KOMOTime komotime = KOMOService.convertDurationToKOMO(rccievent.delay);
        short short1 = komotime.min;
        short short2 = komotime.hour;
        short short3 = komotime.day;
        byte b0 = 0;
        if (this.rgInfoForNextDestination != null) {
            b0 = 0;
        }

        CombiBAPSemiDynamicRouteInfo combibapsemidynamicrouteinfo;
        if (rccievent.hasBetterRoute && rccievent.origin != null) {
            Util.formatDistance((int)rccievent.origin.newRoute.distance, 1);
            long i = (long)Util.getFormattedDistance();
            int j = Util.getFormattedUnit();
            KOMOTime komotime1 = KOMOService.convertTimeToKOMO(
                this.env.getFramework().getKombiTime() + rccievent.origin.newRoute.getEtaWithSpeedAndFlow() + b0
            );
            combibapsemidynamicrouteinfo = new CombiBAPSemiDynamicRouteInfo(
                rccievent.hasTrafficImpactOnCurrentRoute(),
                rccievent.hasBetterRoute,
                short2 + short3 * 24,
                short1,
                i,
                j,
                0,
                1,
                DateMetric.timeFormat == 10 ? 0 : 1,
                komotime1.min,
                komotime1.hour,
                komotime1.day,
                komotime1.month,
                komotime1.year
            );
        } else {
            combibapsemidynamicrouteinfo = new CombiBAPSemiDynamicRouteInfo(
                rccievent.hasTrafficImpactOnCurrentRoute(), rccievent.hasBetterRoute, short2 + short3 * 24, short1
            );
        }

        this.combiBAPListener.setSemidynamicRouteGuidance(combibapsemidynamicrouteinfo);
        this.komoService.setSemiDynRoute(rccievent.hasBetterRoute);
        if (rccievent.hasTrafficImpactOnCurrentRoute()) {
            this.komoService
                .setTrafficOffset(
                    KOMOService.convertTimeFormatToKOMO(DateMetric.timeFormat), short3, short2, short1, true
                );
            if (rccievent.reliable) {
                this.followInfoRIE.trafficInfo.trafficOffset = Util.formatTrafficOffsetDuration(
                    rccievent.delay, 2, this.env
                );
            } else {
                this.followInfoRIE.trafficInfo.trafficOffset = "";
                this.followInfoRIE.estimatedTimeToElement = "--:--";
            }

            this.followInfoRIE.trafficInfo.trafficOffsetAffix = this.env.getTranslatedText(49, "incl.");
        } else {
            this.komoService
                .setTrafficOffset(
                    KOMOService.convertTimeFormatToKOMO(DateMetric.timeFormat), short3, short2, short1, false
                );
            this.followInfoRIE.trafficInfo.trafficOffset = "";
            this.followInfoRIE.trafficInfo.trafficOffsetAffix = "";
        }

        this.updateKOMOFollowInfo();
    }

    public void updateExitView(int i) {
        this.logChannel.log(100000000, "ClusterService#updateExitView( %1 )", i);
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

    public void updateClampState(boolean flag, boolean flag1, boolean flag2, boolean flag3) {
        this.clusterKDKHandler.updateClampState(flag, flag1, flag2, flag3);
    }

    public void showKombiSplashScreen(boolean flag) {
        this.combiBAPListener.showSplashScreen(flag);
    }

    public void updateBapManeuverState(int i) {
        this.combiBAPListener.updateBapManeuverState(i);
    }

    public void applySetupHandlerSettings() {
        this.logChannel.log(10000000, "ClusterService#applySetupHandlerSettings( )");
        this.combiBAPListener.initFromSetup();
    }

    public void setKDKVisibility(boolean flag) {
        this.clusterKDKHandler.setKDKVisibility(flag);
    }

    public void updateManoeuvreViewActive(int i) {
    }

    public void onMagnificationChanged(int i) {
        this.combiBAPListener.onMapScaleChanged(i);
        AbstractMap abstractmap = this.mapInterface.getKombiMap();
        if (abstractmap != null) {
            float[] afloat = abstractmap.getZoomHandler().getZoomList();
            MapScaleInfo mapscaleinfo = this.mapScaleHandler.createMapScaleInfo(i, afloat);
            if (!mapscaleinfo.equals(this.mapScaleTimer.mapScaleInfo)) {
                this.mapScaleTimer.restart(mapscaleinfo);
            }
        }
    }

    public void updateNextManeuver(RouteInfoElement routeinfoelement) {
        this.nextManeuverElement = routeinfoelement;
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

    public void updateGALState(boolean flag) {
        this.combiBAPListener.setGALState(flag);
    }

    public void updateOnlineConnectionState(boolean flag) {
        this.dataConnectivityAvailable = flag;
        this.updateOnlineNavigationState();
    }

    public void updateMapScale(int i, int j, boolean flag) {
        this.logChannel.log(100000000, "ClusterService#updateMapScale( %1, %2, %3 )", i, j, flag);
        boolean[] aboolean = new boolean[]{false};
        boolean[] aboolean1 = new boolean[]{false};
        this.komoService.setMapScale(0, 0, aboolean, i, j, aboolean1, flag);
    }

    public void setHomeAddress(NavLocation navlocation) {
        this.logChannel
            .log(
                10000000,
                "ClusterService#updateHomeAddress() - homeAddress: %1",
                LocationFormatter.formatLocationShort(navlocation)
            );
        if (navlocation == null) {
            this.combiBAPListener.setHomeAddress(null);
        } else {
            this.combiBAPListener.setHomeAddress(this.getBAPNaviDestFromLocation(navlocation));
        }
    }

    public void setProviderChangeFlag(boolean flag) {
        this.satMapProviderChanged = flag;
    }

    private boolean hasSatMapProviderChanged() {
        return this.satMapProviderChanged;
    }

    /* ============================================================
     * CarPlay hook accessors (patched-in; avoid reflection in BAPBridge).
     * Graft onto combined-final stock — env/combiBAPListener/refreshRGIValid
     * are the same members the stock already exposes.
     * ============================================================ */

    public DSIResponseContainer getDSIResponseContainer() {
        return this.env.getContainer();
    }

    public void triggerRefreshRGIValid() {
        this.refreshRGIValid();
    }

    public CombiBAPServiceNavi getCombiBAPListenerCombiService() {
        return this.combiBAPListener.combiservice;
    }

    /** NavigationJobs-only recovery seam used by ScreenNavStatusGate. */
    public void setCombiBAPListenerCombiService(CombiBAPServiceNavi svc) {
        this.combiBAPListener.combiservice = svc;
    }

    /** Restore the authoritative stock INITIALIZING/NORMAL decision after CarPlay releases the
     * cluster.  A deferred-call replay alone is insufficient when no map-ready edge occurred while
     * the takeover was active. */
}
