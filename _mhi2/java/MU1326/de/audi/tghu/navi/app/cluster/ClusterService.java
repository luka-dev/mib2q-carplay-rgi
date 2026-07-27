/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  de.audi.atip.base.IFrameworkAccess
 *  de.audi.atip.hmi.intercommunication.NaviMoKoKDKConstants
 *  de.audi.atip.hmi.model.ModelGroup
 *  de.audi.atip.hmi.modelaccess.ChoiceModelApp
 *  de.audi.atip.hmi.modelaccess.HMIModelApp
 *  de.audi.atip.hmi.modelaccess.MetricsModelApp
 *  de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi
 *  de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNaviListener
 *  de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationInfo
 *  de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviDestination
 *  de.audi.atip.interapp.combi.bap.navi.data.CombiBAPSemiDynamicRouteInfo
 *  de.audi.atip.interapp.combi.ddp2.CombiService
 *  de.audi.atip.interapp.def.NullViewSizeManager
 *  de.audi.atip.interapp.locationaccessor.IMyLocationAccessor
 *  de.audi.atip.log.LogChannel
 *  de.audi.atip.metrics.AbstractMetrics
 *  de.audi.atip.metrics.DateMetric
 *  de.audi.atip.metrics.Distance
 *  de.audi.atip.mmicombi.IViewSizeManager
 *  de.audi.atip.power.PowerEventListener
 *  de.audi.tghu.command.ICommandListFactory
 *  de.audi.tghu.navi.app.NavigationEnv
 *  de.audi.tghu.navi.app.OperationManager
 *  de.audi.tghu.navi.app.SpeechManager
 *  de.audi.tghu.navi.app.audio.AudioStateMachine
 *  de.audi.tghu.navi.app.cluster.BAPDistanceFormatter
 *  de.audi.tghu.navi.app.cluster.BAPDistanceFormatter$BAPDistance
 *  de.audi.tghu.navi.app.cluster.ClusterInputListener
 *  de.audi.tghu.navi.app.cluster.ClusterKDKHandler
 *  de.audi.tghu.navi.app.cluster.ClusterKDKHandlerImpl
 *  de.audi.tghu.navi.app.cluster.ClusterViewMode
 *  de.audi.tghu.navi.app.cluster.CombiBAPListener
 *  de.audi.tghu.navi.app.cluster.KOMOService
 *  de.audi.tghu.navi.app.cluster.KOMOTime
 *  de.audi.tghu.navi.app.interapp.IViewSizeChangeHandler
 *  de.audi.tghu.navi.app.interapp.NullViewSizeChangeHandler
 *  de.audi.tghu.navi.app.map.AbstractMap
 *  de.audi.tghu.navi.app.map.MapInterface
 *  de.audi.tghu.navi.app.map.MapManager
 *  de.audi.tghu.navi.app.map.handler.MapScaleHandler
 *  de.audi.tghu.navi.app.map.handler.MapScaleInfo
 *  de.audi.tghu.navi.app.map.handler.MapScaleTimer
 *  de.audi.tghu.navi.app.map.routecalc.RcciEvent
 *  de.audi.tghu.navi.app.rp.TripHandler$TripData
 *  de.audi.tghu.navi.app.util.LocationFormatter
 *  de.audi.tghu.navi.app.util.Util
 *  de.audi.tghu.navi.app.util.addressformatting.AddressFormatter
 *  de.audi.tghu.navi.app.util.addressformatting.LocationFormattingResponse
 *  de.esolutions.fw.util.commons.Buffer
 *  org.dsi.ifc.global.NavLocation
 *  org.dsi.ifc.komoview.RouteInfoElement
 *  org.dsi.ifc.komoview.TrafficInfo
 *  org.dsi.ifc.navigation.BapManeuverDescriptor
 *  org.dsi.ifc.navigation.BapTurnToInfo
 *  org.dsi.ifc.navigation.DistanceToNextManeuver
 *  org.dsi.ifc.navigation.NavLaneGuidanceData
 *  org.dsi.ifc.navigation.PosPosition
 *  org.dsi.ifc.navigation.RgInfoForNextDestination
 *  org.dsi.ifc.tmc.TmcMessage
 */
package de.audi.tghu.navi.app.cluster;

import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.intercommunication.NaviMoKoKDKConstants;
import de.audi.atip.hmi.model.ModelGroup;
import de.audi.atip.hmi.modelaccess.ChoiceModelApp;
import de.audi.atip.hmi.modelaccess.HMIModelApp;
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
import de.audi.atip.metrics.AbstractMetrics;
import de.audi.atip.metrics.DateMetric;
import de.audi.atip.metrics.Distance;
import de.audi.atip.mmicombi.IViewSizeManager;
import de.audi.atip.power.PowerEventListener;
import de.audi.tghu.command.ICommandListFactory;
import de.audi.tghu.navi.app.NavigationEnv;
import de.audi.tghu.navi.app.OperationManager;
import de.audi.tghu.navi.app.SpeechManager;
import de.audi.tghu.navi.app.audio.AudioStateMachine;
import de.audi.tghu.navi.app.cluster.BAPDistanceFormatter;
import de.audi.tghu.navi.app.cluster.ClusterInputListener;
import de.audi.tghu.navi.app.cluster.ClusterKDKHandler;
import de.audi.tghu.navi.app.cluster.ClusterKDKHandlerImpl;
import de.audi.tghu.navi.app.cluster.ClusterViewMode;
import de.audi.tghu.navi.app.cluster.CombiBAPListener;
import de.audi.tghu.navi.app.cluster.KOMOService;
import de.audi.tghu.navi.app.cluster.KOMOTime;
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

public class ClusterService
implements NaviMoKoKDKConstants,
PowerEventListener {
    public static final String EMPTY_STREET_LABEL = "---";
    protected LogChannel logChannel;
    protected final NavigationEnv env;
    private boolean rgiDataValid = false;
    private final DateMetric etaDateMetric;
    private final DateMetric rttDateMetric;
    private Distance distanceToManeuver = new Distance(0.0f, 1);
    private Distance distanceToDestination = new Distance(0.0f, 1);
    private ClusterViewMode clusterViewMode;
    protected ModelGroup travelParametersGroup;
    private ModelGroup nextManeuverGroup;
    private boolean showBargraph = false;
    private String turnToStreet = "isNativeLittleEndian";
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

    public ClusterService(NavigationEnv navigationEnv, SpeechManager speechManager, OperationManager operationManager, AudioStateMachine audioStateMachine, MapManager mapManager, ICommandListFactory iCommandListFactory) {
        this(navigationEnv, speechManager, operationManager, audioStateMachine, mapManager, iCommandListFactory, (IViewSizeManager)new NullViewSizeManager(navigationEnv.getClusterLogChannel()), (IViewSizeChangeHandler)new NullViewSizeChangeHandler());
    }

    protected ClusterKDKHandler initClusterKDKHandler(IViewSizeChangeHandler iViewSizeChangeHandler) {
        return new ClusterKDKHandlerImpl(this.env, iViewSizeChangeHandler, this.combiBAPListener);
    }

    public ClusterService(NavigationEnv navigationEnv, SpeechManager speechManager, OperationManager operationManager, AudioStateMachine audioStateMachine, MapManager mapManager, ICommandListFactory iCommandListFactory, IViewSizeManager iViewSizeManager, IViewSizeChangeHandler iViewSizeChangeHandler) {
        this.env = navigationEnv;
        this.mapInterface = mapManager.getMapInterface();
        this.logChannel = navigationEnv.getClusterLogChannel();
        this.bapDistanceFormatter = new BAPDistanceFormatter(this.logChannel);
        this.travelParametersGroup = new ModelGroup();
        this.nextManeuverGroup = new ModelGroup();
        this.etaDateMetric = new DateMetric(new Date(), 1);
        this.rttDateMetric = new DateMetric(new Date(), 3);
        this.followInfoRIE = new RouteInfoElement(null, null, 0, null, null, null, 0, null, null, null, null, new TrafficInfo(), 0, null, null, null, null, 0);
        this.combiBAPListener = this.initBAPListener(speechManager, operationManager, audioStateMachine, mapManager, iCommandListFactory, iViewSizeManager);
        this.clusterViewMode = new ClusterViewMode(navigationEnv, this);
        this.clusterKDKHandler = this.initClusterKDKHandler(iViewSizeChangeHandler);
        this.komoService = new KOMOService(navigationEnv, this, this.clusterKDKHandler);
        this.clusterInputListener = this.createClusterInputListener(navigationEnv);
        this.mapScaleHandler = new MapScaleHandler();
        this.mapScaleTimer = new MapScaleTimer(navigationEnv, this, this.mapScaleHandler);
        navigationEnv.getLabelModel(62).setText("isNativeLittleEndian");
        this.initializeModels();
    }

    private void initializeModels() {
        this.logChannel.log(10000000, "ClusterService#initializeModels() ");
        this.env.getLabelModel(71).setText("isNativeLittleEndian");
        this.env.getChoiceModel(69).setValue(0);
        this.turnToStreetValid = false;
        this.turnToStreet = "isNativeLittleEndian";
        this.env.getMetricsModel(66).setStatus(3);
        this.env.getMetricsModel(66).setMetric((AbstractMetrics)this.etaDateMetric);
        this.env.getMetricsModel(63).setStatus(3);
        this.env.getMetricsModel(63).setMetric((AbstractMetrics)this.distanceToDestination);
        this.travelParametersGroup.add((HMIModelApp)this.env.getMetricsModel(66));
        this.travelParametersGroup.add((HMIModelApp)this.env.getMetricsModel(63));
        this.env.getMetricsModel(64).setStatus(3);
        this.env.getMetricsModel(64).setMetric((AbstractMetrics)this.distanceToManeuver);
        this.env.getChoiceModel(65).setValue(-1);
        this.showBargraph = false;
        this.nextManeuverGroup.add((HMIModelApp)this.env.getMetricsModel(64));
        this.nextManeuverGroup.add((HMIModelApp)this.env.getChoiceModel(65));
        if (Util.isClusterKDKOnly((IFrameworkAccess)this.env.getFramework())) {
            this.switchDisplayContextKombi(9);
        } else if (Util.isClusterMapAvailable((IFrameworkAccess)this.env.getFramework())) {
            this.switchDisplayContextKombi(8);
        }
    }

    protected CombiBAPListener initBAPListener(SpeechManager speechManager, OperationManager operationManager, AudioStateMachine audioStateMachine, MapManager mapManager, ICommandListFactory iCommandListFactory, IViewSizeManager iViewSizeManager) {
        return new CombiBAPListener(this, this.logChannel, this.env, speechManager, operationManager, audioStateMachine, mapManager, iCommandListFactory, iViewSizeManager);
    }

    public synchronized void unitsChanged(TripHandler.TripData tripData) {
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
        this.logChannel.log(100000000, "ClusterService#updateCurrentStreet( %1 )", (Object)string);
        String string2 = string;
        String string3 = string;
        String string4 = string;
        if (Util.isEmpty((String)string2)) {
            this.logChannel.log(10000000, "ClusterService#updateCurrentStreet() - invalid currentStreet: %1", (Object)string2);
            string3 = "isNativeLittleEndian";
            string2 = "isNativeLittleEndian";
            string4 = EMPTY_STREET_LABEL;
        }
        this.env.getLabelModel(62).setText(string4);
        this.komoService.setCurrentStreet(string2);
        this.combiBAPListener.setCurrentStreet(string3);
    }

    public void updateTurnToStreet(String string, boolean bl) {
        this.logChannel.log(100000000, "ClusterService#updateTurnToStreet( %1 )", (Object)string);
        this.turnToStreet = string;
        this.turnToStreetValid = !Util.isEmpty((String)string);
        this.env.getLabelModel(71).setText(string);
        this.refreshStreetMode();
    }

    private void refreshStreetMode() {
        this.logChannel.log(100000000, "ClusterService#refreshStreetMode() - turnToStreetValid: %1, showBargraph: %2 ", this.turnToStreetValid, this.showBargraph);
        ChoiceModelApp choiceModelApp = this.env.getChoiceModel(69);
        if (this.turnToStreetValid && this.showBargraph) {
            choiceModelApp.setValue(1);
            this.komoService.setTurnToStreet(this.turnToStreet, "isNativeLittleEndian");
        } else {
            choiceModelApp.setValue(0);
            this.komoService.setTurnToStreet("isNativeLittleEndian", "isNativeLittleEndian");
        }
    }

    public void refreshDistanceToNextManeuver() {
        DistanceToNextManeuver distanceToNextManeuver = this.env.getContainer().getDistanceToNextManeuver();
        this.refreshDistanceToNextManeuver(distanceToNextManeuver);
    }

    private int convertBAP2KOMODistanceUnit(int n) {
        switch (n) {
            case 0: {
                return 1;
            }
            case 1: 
            case 6: {
                return 2;
            }
            case 2: {
                return 3;
            }
            case 3: {
                return 4;
            }
            case 4: 
            case 7: {
                return 0;
            }
            case 5: {
                return 5;
            }
        }
        this.logChannel.log(100000, "ClusterService#convertBAP2KOMOUnit() - unknown or not convertable BAP unit: %1", (long)n);
        return 255;
    }

    protected void refreshDistanceToNextManeuver(DistanceToNextManeuver distanceToNextManeuver) {
        boolean bl;
        int n;
        long l;
        BAPDistanceFormatter.BAPDistance bAPDistance;
        if (distanceToNextManeuver == null) {
            this.logChannel.log(1000000, "ClusterService#refreshDistanceToNextManeuver() - distanceToNextManeuver is null! ");
            return;
        }
        this.logChannel.log(100000000, "ClusterService#refreshDistanceToNextManeuver() - distanceToNextManeuver: %1", (Object)distanceToNextManeuver);
        boolean bl2 = distanceToNextManeuver.showDistance;
        int n2 = distanceToNextManeuver.distance;
        this.showBargraph = distanceToNextManeuver.showBargraph;
        int n3 = distanceToNextManeuver.bargraph;
        if (n2 > 0) {
            bAPDistance = this.bapDistanceFormatter.formatDistanceToTurn(n2, Distance.getSystemUnit() == 1);
            l = bAPDistance.getValue();
            n = this.convertBAP2KOMODistanceUnit(bAPDistance.getUnit());
            bl = !this.showBargraph;
        } else {
            l = -1L;
            n = 255;
            bl = false;
        }
        bAPDistance = this.env.getMetricsModel(64);
        if (bl2 && n2 > 0) {
            this.distanceToManeuver.setValue((float)n2 / 1000.0f);
            bAPDistance.setMetric((AbstractMetrics)this.distanceToManeuver);
        }
        if (bl2 && n2 > 0 && !this.showBargraph) {
            Util.setModelStatus((HMIModelApp)bAPDistance, (int)1);
        } else {
            Util.setModelStatus((HMIModelApp)bAPDistance, (int)3);
        }
        int n4 = this.showBargraph ? n3 : -1;
        this.env.getChoiceModel(65).setValue(n4);
        this.refreshStreetMode();
        this.nextManeuverGroup.flush();
        this.refreshDistanceToNextManeuverMOST(l, n, bl, bl2);
        this.combiBAPListener.setDistanceToNextManeuver(n2, this.showBargraph, n3);
    }

    public void refreshTravelParameters(TripHandler.TripData tripData) {
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
            routeInfoElement.distanceToElement = "isNativeLittleEndian";
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

    protected void refreshDistanceToNextManeuverMOST(long l, int n, boolean bl, boolean bl2) {
        this.komoService.setDistanceToNextManeuver(l, n, bl);
    }

    protected int getDestIndex() {
        int n = this.mapInterface.getMap().getNaviInterface().getRouteListLength();
        int n2 = this.mapInterface.getMap().getNaviInterface().getIndexOfCurrentDestination();
        if (n2 + 1 < n) {
            return 0;
        }
        return -1;
    }

    protected void updateArrivalTime(boolean bl, long l, boolean bl2) {
        String string;
        this.logChannel.log(100000000, "ClusterService#updateArrivalTime( %1, %2 )", bl, l);
        MetricsModelApp metricsModelApp = this.env.getMetricsModel(66);
        if (bl) {
            this.etaDateMetric.setDate(l);
            metricsModelApp.setMetric((AbstractMetrics)this.etaDateMetric);
            Util.setModelStatus((HMIModelApp)metricsModelApp, (int)1);
            string = Util.formatTime((long)l, (int)2, (NavigationEnv)this.env);
        } else {
            this.logChannel.log(10000000, "ClusterService#updateArrivalTime() - invalid flag for ETA set! ");
            Util.setModelStatus((HMIModelApp)metricsModelApp, (int)3);
            string = "--:--";
        }
        int n = KOMOService.convertTimeFormatToKOMO((int)DateMetric.timeFormat);
        KOMOTime kOMOTime = KOMOService.convertTimeToKOMO((long)l);
        this.followInfoRIE.estimatedTimeToElement = string;
        this.komoService.setETA(n, kOMOTime.day, kOMOTime.hour, kOMOTime.min, bl, bl2);
        this.combiBAPListener.setRgTimeToNextDestination(l, true, bl);
    }

    protected void updateRemainingTravelTime(boolean bl, long l) {
        this.logChannel.log(100000000, "ClusterService#RemainingTravelTime( %1, %2 )", bl, l);
        MetricsModelApp metricsModelApp = this.env.getMetricsModel(66);
        if (bl) {
            this.rttDateMetric.setDate(l);
            metricsModelApp.setMetric((AbstractMetrics)this.rttDateMetric);
            Util.setModelStatus((HMIModelApp)metricsModelApp, (int)1);
        } else {
            this.logChannel.log(10000000, "ClusterService#updateArrivalTime() - invalid flag for ETA set! ");
            Util.setModelStatus((HMIModelApp)metricsModelApp, (int)3);
        }
        KOMOTime kOMOTime = KOMOService.convertDurationToKOMO((long)l);
        this.komoService.setRTT(kOMOTime.hour, kOMOTime.min, bl);
        this.combiBAPListener.setRgTimeToNextDestination(l, false, bl);
    }

    protected void updateDistanceToDestination(int n, boolean bl) {
        boolean bl2;
        String string;
        this.logChannel.log(100000000, "ClusterService#updateDistanceToDestination( %1 )", (long)n);
        long l = -1L;
        int n2 = -1;
        MetricsModelApp metricsModelApp = this.env.getMetricsModel(63);
        if (n > 0) {
            this.distanceToDestination.setValue((float)n / 1000.0f);
            metricsModelApp.setMetric((AbstractMetrics)this.distanceToDestination);
            Util.setModelStatus((HMIModelApp)metricsModelApp, (int)1);
            string = Util.formatDistance((int)n, (int)1, (int)2, (String)EMPTY_STREET_LABEL);
            BAPDistanceFormatter.BAPDistance bAPDistance = this.bapDistanceFormatter.formatDistanceToDestination(n, Distance.getSystemUnit() == 1);
            l = bAPDistance.getValue();
            n2 = this.convertBAP2KOMODistanceUnit(bAPDistance.getUnit());
            bl2 = true;
        } else {
            this.logChannel.log(10000000, "ClusterService#updateDistanceToDestination() - invalid distanceToDestination! ");
            Util.setModelStatus((HMIModelApp)metricsModelApp, (int)3);
            string = "isNativeLittleEndian";
            bl2 = false;
        }
        this.followInfoRIE.distanceToElement = string;
        this.komoService.setDistanceToDestination(l, n2, bl2);
        this.combiBAPListener.setRgDistanceToNextDestination(n, bl);
    }

    public void updateSoPosPosition(PosPosition posPosition) {
        this.logChannel.log(100000000, "ClusterService#updateSoPosPosition( %1 )", (Object)posPosition);
        short s = 0;
        short s2 = 255;
        int n = -1;
        if (posPosition != null) {
            s = (short)posPosition.getDirectionAngle();
            s2 = (short)posPosition.getDirectionSymbolic();
            n = posPosition.getState();
        } else {
            this.logChannel.log(100000, "ClusterService#updateSoPosPosition() - invalid soPosPosition!");
        }
        this.combiBAPListener.setVehicleHeading(s, s2);
        this.combiBAPListener.setInfoStateGPS(n);
    }

    public void updateSoPosPositionDescription(NavLocation navLocation) {
        this.logChannel.log(100000000, "ClusterService#updateSoPosPositionDescription( %1 )", (Object)navLocation);
        String string = LocationFormatter.formatCity((NavLocation)navLocation);
        this.komoService.setCityName(string);
    }

    public void updateRgDirectionToNextDestination(short s) {
        this.logChannel.log(100000000, "ClusterService#updateRgDirectionToNextDestination( %1 )", (long)s);
    }

    public void updateRGIString(short[] sArray) {
        if (sArray != null && sArray.length > 0) {
            this.rgiDataValid = true;
            this.env.getDataModel(68).set((Object)sArray);
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
            this.updateTurnToStreet("isNativeLittleEndian", false);
            this.clearRouteInfoElement(this.followInfoRIE);
            this.clearRouteInfoElement(this.nextManeuverElement);
            this.updateDestinationInfo(null, 0, 0);
        }
        this.clusterKDKHandler.updateRgActive(bl);
        this.combiBAPListener.setRgActive(bl);
    }

    public void updateNavState(int n, int n2) {
        this.logChannel.log(10000000, "ClusterService#updateNavState( %1, %2 )", (long)n, (long)n2);
        if (this.combiDDP2ServiceNavi != null) {
            this.combiDDP2ServiceNavi.updateNavInitialized(n, n2);
        }
    }

    private void refreshRGIValid() {
        boolean bl = this.env.getContainer().isRgActive();
        boolean bl2 = bl && this.rgiDataValid;
        this.logChannel.log(100000000, "ClusterService#refreshRGIValid() - rgActive: %1, rgiDataValid: %2", bl, this.rgiDataValid);
        this.clusterViewMode.setRGIValid(bl2);
    }

    public void refreshViewMode(int n) {
        this.combiBAPListener.setViewMode(n);
    }

    public void updateLaneGuidance(NavLaneGuidanceData[] navLaneGuidanceDataArray, boolean bl) {
        this.combiBAPListener.setLaneGuidance(navLaneGuidanceDataArray, bl);
    }

    public void updateManeuverDescriptor(BapManeuverDescriptor[] bapManeuverDescriptorArray) {
        this.rgiDataValid = bapManeuverDescriptorArray != null && bapManeuverDescriptorArray.length > 0;
        this.refreshRGIValid();
        this.combiBAPListener.setManeuverDescriptor(bapManeuverDescriptorArray);
    }

    public void updateManeuverDescriptor(BapManeuverDescriptor[] bapManeuverDescriptorArray, int n) {
        this.rgiDataValid = bapManeuverDescriptorArray != null && bapManeuverDescriptorArray.length > 0;
        this.refreshRGIValid();
        this.combiBAPListener.setManeuverDescriptor(bapManeuverDescriptorArray, n);
    }

    public String toString() {
        Buffer buffer = new Buffer();
        buffer.append((Object)this.komoService);
        buffer.append((Object)this.clusterViewMode);
        return buffer.toString();
    }

    public void updateBapTurnToInfo(BapTurnToInfo[] bapTurnToInfoArray) {
        this.combiBAPListener.setTurnToInfo(bapTurnToInfoArray);
    }

    public void updateInfoStatesGPS(int n) {
        this.combiBAPListener.setInfoStateGPS(n);
    }

    private boolean initScreenNeededOnKombi() {
        boolean bl = this.env.getContainer().getNavstateOfOperation() == 5;
        boolean bl2 = Util.isClusterMapAvailable((IFrameworkAccess)this.env.getFramework());
        boolean bl3 = false;
        AbstractMap abstractMap = this.mapInterface.getKombiMap();
        if (abstractMap != null && abstractMap.isInitialized()) {
            bl3 = true;
        }
        if (bl && !bl3 && bl2) {
            return true;
        }
        return !this.operationStateIsKnownToTheKombi;
    }

    public synchronized void updateOperationState(int n) {
        this.logChannel.log(1000000, "ClusterService#updateOperationState( %1 )", (long)n);
        this.operationStateIsKnownToTheKombi = this.combiBAPListener.setInfoStateNavi(n);
        this.combiBAPListener.forceShowInitScreen(this.initScreenNeededOnKombi());
        if (this.combiBAPListener.lvdsMapVisible && this.env.getContainer().getNavstateOfOperation() == 5) {
            this.showKombiMap(true);
        }
    }

    public void updateXUrgentMessages(TmcMessage[] tmcMessageArray) {
        this.combiBAPListener.setXUrgentMessages(tmcMessageArray);
    }

    public void updateMessagesOnRoute(TmcMessage[] tmcMessageArray) {
        this.combiBAPListener.setMessagesOnRoute(tmcMessageArray);
    }

    public void setRouteGuidanceAborted() {
        this.combiBAPListener.setRouteGuidanceAborted();
    }

    public void updateRgInfoForNextDestination(RgInfoForNextDestination rgInfoForNextDestination) {
        this.logChannel.log(100000000, "ClusterService#updateRgInfoForNextDestination(%1)", (Object)rgInfoForNextDestination);
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

    public void setSupplementaryMap(int n, boolean bl) {
        this.logChannel.log(10000000, "ClusterService#setSupplementaryMap() - supplementaryMapView: %2, visible: %1", bl, (long)n);
        if (n != 1 && bl) {
            this.logChannel.log(100000, "ClusterService#setSupplementaryMap() - got request to show supplementary map although not available or unimplemented");
        }
        this.setKDKVisibility(bl);
    }

    public void switchDisplayContextKombi(int n) {
        this.logChannel.log(1000000, "ClusterService#switchDisplayContextKombi( %1 )", (long)n);
        this.mapInterface.switchDisplayContextKombi(n);
    }

    public void setKOMODataRate(int n) {
        if (Util.isClusterMapMOST((IFrameworkAccess)this.env.getFramework())) {
            this.setKOMODataRate(n, true);
        }
    }

    public int getKOMODataRate() {
        boolean bl;
        this.logChannel.log(100000000, "ClusterService#getKOMODataRate()");
        ChoiceModelApp choiceModelApp = this.env.getChoiceModel(1, 168);
        int n = choiceModelApp.getHints();
        boolean bl2 = (n & 2) == 2;
        boolean bl3 = bl = (n & 1) == 1;
        if (bl2) {
            if (bl) {
                this.logChannel.log(100000, "ClusterService#getKOMODataRate() - undefined state: full and reduced framerate -> assuming full framerate");
            }
            return 2;
        }
        if (bl) {
            return 1;
        }
        return 0;
    }

    /*
     * WARNING - Removed try catching itself - possible behaviour change.
     */
    public void reSyncKOMO() {
        this.logChannel.log(1000000, "ClusterService#reSyncKOMO()");
        Object object = this.komoDataRateMutex;
        synchronized (object) {
            int n = this.getKOMODataRate();
            this.setKOMODataRate(0);
            this.setKOMODataRate(n);
            this.clusterViewMode.refreshRgMode();
        }
    }

    /*
     * WARNING - Removed try catching itself - possible behaviour change.
     */
    private void setKOMODataRate(int n, boolean bl) {
        this.logChannel.log(1000000, "ClusterService#setKOMODataRate( %1 )", (long)n);
        Object object = this.komoDataRateMutex;
        synchronized (object) {
            ChoiceModelApp choiceModelApp = this.env.getChoiceModel(1, 168);
            if (n == 2) {
                choiceModelApp.removeHint(1);
                choiceModelApp.addHint(2);
            } else if (n == 1) {
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
        if (this.komoService != null && Util.isKOMOFollowMode((IFrameworkAccess)this.env.getFramework())) {
            try {
                this.logChannel.log(100000000, "ClusterService#updateKOMOFollowInfo() - followInfoRIE: %1, nextManeuverElement: %2", (Object)this.followInfoRIE, (Object)this.nextManeuverElement);
                if (Util.isSetRouteInfoDSIAvailable((IFrameworkAccess)this.env.getFramework())) {
                    this.komoService.setRouteInfo(new RouteInfoElement[]{this.followInfoRIE, this.nextManeuverElement});
                } else {
                    this.komoService.setRouteInfoElement(this.followInfoRIE);
                }
            }
            catch (Exception exception) {
                this.logChannel.log(10000, "ClusterService#updateFollowInfo() - ERROR=%1", (Throwable)exception);
            }
        }
    }

    public void updateAltitude(int n) {
        this.logChannel.log(100000000, "ClusterService#updateAltitude( %1 )", (long)n);
        this.combiBAPListener.setAltitude(n);
    }

    private String getDestinationDescription4BAP(NavLocation navLocation) {
        if (navLocation == null || !navLocation.isPositionValid()) {
            return "isNativeLittleEndian";
        }
        try {
            LocationFormattingResponse locationFormattingResponse = AddressFormatter.formatTwoLines((NavLocation)navLocation, (NavigationEnv)this.env);
            String string = locationFormattingResponse.getFirstLineAsText();
            return string != null ? string : "isNativeLittleEndian";
        }
        catch (Exception exception) {
            this.env.getLogChannel().log(100000, "Util#getString4BAPFromNavLocation - got an exception from AddressFormatter#formatTwoLines: %1", (Throwable)exception);
            return "isNativeLittleEndian";
        }
    }

    private CombiBAPNaviDestination getBAPNaviDestFromLocation(NavLocation navLocation) {
        if (navLocation == null) {
            return new CombiBAPNaviDestination();
        }
        IMyLocationAccessor iMyLocationAccessor = Util.getLocationAccessor((NavLocation)navLocation);
        String string = this.getDestinationDescription4BAP(navLocation);
        if (Util.isEmpty((String)string)) {
            string = Util.isEmpty((String)iMyLocationAccessor.getPoiName()) ? null : iMyLocationAccessor.getPoiName();
        }
        return new CombiBAPNaviDestination(null, null, Util.isEmpty((String)navLocation.getStreet()) ? null : navLocation.getStreet(), Util.isEmpty((String)navLocation.getHousenumber()) ? null : navLocation.getHousenumber(), Util.isEmpty((String)navLocation.getTown()) ? null : navLocation.getTown(), Util.isEmpty((String)navLocation.getTownRefinement()) ? null : navLocation.getTownRefinement(), Util.isEmpty((String)iMyLocationAccessor.getState()) ? null : iMyLocationAccessor.getState(), Util.isEmpty((String)navLocation.getZipCode()) ? null : navLocation.getZipCode(), Util.isEmpty((String)navLocation.getCountry()) ? null : navLocation.getCountry(), ClusterService.wgs84ToDeg(navLocation.getLatitude()), ClusterService.wgs84ToDeg(navLocation.getLongitude()), iMyLocationAccessor.getType() == 1 ? 255 : 0, string, Util.isEmpty((String)iMyLocationAccessor.getPoiCategory()) ? null : iMyLocationAccessor.getPoiCategory(), 255);
    }

    public void updateDestinationInfo(NavLocation navLocation, int n, int n2) {
        CombiBAPDestinationInfo combiBAPDestinationInfo;
        if (this.logChannel.isDebug2()) {
            this.logChannel.log(100000000, "ClusterService#updateDestinationInfo() - noOfStopovers: %2, noOfNextStopover: %3, nextDestination: %1", (Object)LocationFormatter.formatLocationShort((NavLocation)navLocation), (long)n, (long)n2);
        }
        CombiBAPNaviDestination combiBAPNaviDestination = this.getBAPNaviDestFromLocation(navLocation);
        if (navLocation != null) {
            combiBAPDestinationInfo = new CombiBAPDestinationInfo(combiBAPNaviDestination);
            combiBAPDestinationInfo.setStopoverInformation(n, n2);
        } else {
            combiBAPDestinationInfo = new CombiBAPDestinationInfo(combiBAPNaviDestination);
            combiBAPDestinationInfo.setStopoverInformation(0, 0);
        }
        this.combiBAPListener.setDestinationInfo(combiBAPDestinationInfo);
    }

    public void updateSemidynamicRouteGuidance(RcciEvent rcciEvent) {
        CombiBAPSemiDynamicRouteInfo combiBAPSemiDynamicRouteInfo;
        this.logChannel.log(10000000, "ClusterService#updateSemidynamicRouteGuidance() - TrafficImpactOnCurrentRoute: %1, delay: %2", rcciEvent.hasTrafficImpactOnCurrentRoute(), rcciEvent.delay);
        KOMOTime kOMOTime = KOMOService.convertDurationToKOMO((long)rcciEvent.delay);
        short s = kOMOTime.min;
        short s2 = kOMOTime.hour;
        short s3 = kOMOTime.day;
        int n = 0;
        if (this.rgInfoForNextDestination != null) {
            n = 0;
        }
        if (rcciEvent.hasBetterRoute && rcciEvent.origin != null) {
            Util.formatDistance((int)((int)rcciEvent.origin.newRoute.distance), (int)1);
            long l = (long)Util.getFormattedDistance();
            int n2 = Util.getFormattedUnit();
            KOMOTime kOMOTime2 = KOMOService.convertTimeToKOMO((long)(this.env.getFramework().getKombiTime() + rcciEvent.origin.newRoute.getEtaWithSpeedAndFlow() + (long)n));
            combiBAPSemiDynamicRouteInfo = new CombiBAPSemiDynamicRouteInfo(rcciEvent.hasTrafficImpactOnCurrentRoute(), rcciEvent.hasBetterRoute, s2 + s3 * 24, (int)s, l, n2, 0, 1, DateMetric.timeFormat == 10 ? 0 : 1, (int)kOMOTime2.min, (int)kOMOTime2.hour, (int)kOMOTime2.day, (int)kOMOTime2.month, (int)kOMOTime2.year);
        } else {
            combiBAPSemiDynamicRouteInfo = new CombiBAPSemiDynamicRouteInfo(rcciEvent.hasTrafficImpactOnCurrentRoute(), rcciEvent.hasBetterRoute, s2 + s3 * 24, (int)s);
        }
        this.combiBAPListener.setSemidynamicRouteGuidance(combiBAPSemiDynamicRouteInfo);
        this.komoService.setSemiDynRoute(rcciEvent.hasBetterRoute);
        if (rcciEvent.hasTrafficImpactOnCurrentRoute()) {
            this.komoService.setTrafficOffset(KOMOService.convertTimeFormatToKOMO((int)DateMetric.timeFormat), s3, s2, s, true);
            if (rcciEvent.reliable) {
                this.followInfoRIE.trafficInfo.trafficOffset = Util.formatTrafficOffsetDuration((long)rcciEvent.delay, (int)2, (NavigationEnv)this.env);
            } else {
                this.followInfoRIE.trafficInfo.trafficOffset = "isNativeLittleEndian";
                this.followInfoRIE.estimatedTimeToElement = "--:--";
            }
            this.followInfoRIE.trafficInfo.trafficOffsetAffix = this.env.getTranslatedText(49, "incl.");
        } else {
            this.komoService.setTrafficOffset(KOMOService.convertTimeFormatToKOMO((int)DateMetric.timeFormat), s3, s2, s, false);
            this.followInfoRIE.trafficInfo.trafficOffset = "isNativeLittleEndian";
            this.followInfoRIE.trafficInfo.trafficOffsetAffix = "isNativeLittleEndian";
        }
        this.updateKOMOFollowInfo();
    }

    public void updateExitView(int n) {
        this.logChannel.log(100000000, "ClusterService#updateExitView( %1 )", (long)n);
        this.combiBAPListener.setExitView(n);
    }

    public void notifyPowerListenerOnEnterState(int n, int n2) {
        this.clusterKDKHandler.notifyPowerListenerOnEnterState(n, n2);
    }

    public void notifyPowerListenerOnExitState(int n, int n2) {
        this.clusterKDKHandler.notifyPowerListenerOnExitState(n, n2);
    }

    public void notifyPowerTriggerAction(int n, int n2) {
        this.clusterKDKHandler.notifyPowerTriggerAction(n, n2);
    }

    public void updateClampState(boolean bl, boolean bl2, boolean bl3, boolean bl4) {
        this.clusterKDKHandler.updateClampState(bl, bl2, bl3, bl4);
    }

    public void showKombiSplashScreen(boolean bl) {
        this.combiBAPListener.showSplashScreen(bl);
    }

    public void updateBapManeuverState(int n) {
        this.combiBAPListener.updateBapManeuverState(n);
    }

    public void applySetupHandlerSettings() {
        this.logChannel.log(10000000, "ClusterService#applySetupHandlerSettings( )");
        this.combiBAPListener.initFromSetup();
    }

    public void setKDKVisibility(boolean bl) {
        this.clusterKDKHandler.setKDKVisibility(bl);
    }

    public void updateManoeuvreViewActive(int n) {
    }

    public void onMagnificationChanged(int n) {
        float[] fArray;
        MapScaleInfo mapScaleInfo;
        this.combiBAPListener.onMapScaleChanged(n);
        AbstractMap abstractMap = this.mapInterface.getKombiMap();
        if (abstractMap != null && !(mapScaleInfo = this.mapScaleHandler.createMapScaleInfo(n, fArray = abstractMap.getZoomHandler().getZoomList())).equals((Object)this.mapScaleTimer.mapScaleInfo)) {
            this.mapScaleTimer.restart(mapScaleInfo);
        }
    }

    public void updateNextManeuver(RouteInfoElement routeInfoElement) {
        this.nextManeuverElement = routeInfoElement;
        this.updateKOMOFollowInfo();
    }

    public static float wgs84ToDeg(int n) {
        return (float)n / 1.1930464E7f;
    }

    public void setSupportedSupplementaryMapView(int n) {
        this.logChannel.log(10000000, "ClusterService#setSupportedSupplementaryMapView()");
    }

    public void updateOnlineNavigationState() {
        int n;
        if (this.mapInterface.getKombiMap() != null && this.mapInterface.getKombiMap().getSetup() != null) {
            n = this.mapInterface.getKombiMap().getSetup().getMapRepresentation();
        } else {
            this.logChannel.log(10000000, "ClusterService#updateOnlineNavigationState() - Kombi map does not exist or not initialized yet!");
            n = 0;
        }
        int n2 = this.env.getChoiceModel(162).getValue();
        this.logChannel.log(10000000, "ClusterService#updateOnlineNavigationState() - mapRepresentation: %1, bufferProgress: %2, dataConnectivityAvailable: %3", (long)n, (long)n2, true);
        this.combiBAPListener.setOnlineNavigationState(n == 1 && !this.hasSatMapProviderChanged(), n2, true);
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

    public void updateMapScale(int n, int n2, boolean bl) {
        this.logChannel.log(100000000, "ClusterService#updateMapScale( %1, %2, %3 )", (long)n, (long)n2, bl);
        boolean[] blArray = new boolean[]{false};
        boolean[] blArray2 = new boolean[]{false};
        this.komoService.setMapScale(0, 0, blArray, n, n2, blArray2, bl);
    }

    public void setHomeAddress(NavLocation navLocation) {
        this.logChannel.log(10000000, "ClusterService#updateHomeAddress() - homeAddress: %1", (Object)LocationFormatter.formatLocationShort((NavLocation)navLocation));
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
}
