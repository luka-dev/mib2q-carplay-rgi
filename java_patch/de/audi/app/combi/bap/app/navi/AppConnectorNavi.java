/*
 * AppConnectorNavi
 *
 * PATCHED CLASS - attempts to replace original in lsd.jxe.
 *
 * NOTE: class-replacement via jar in /mnt/app/eso/hmi/lsd/jars/ may not
 * actually shadow the stock class inside lsd.jxe (OSGi bundle / classloader
 * isolation). If class-replacement WORKS, the gate below will intercept
 * HMI's own updateMapPresentation calls during CarPlay. If it DOESN'T
 * work, that's fine — CarPlayHook also pushes directly through the live
 * stock CombiBAPServiceNavi which achieves the same goal without relying
 * on class shadowing (see CarPlayHook.naviServiceRef / onDeviceStateUpdate).
 *
 * The gate here is belt-and-suspenders: no harm if active, no harm if not.
 *
 * Purpose:
 *   Hide the map-tab flap menus (left/right) on the Virtual Cockpit while
 *   CarPlay is active.
 *
 *   BAP FctID 54 Map_Presentation_Status mirrors MMI's flap state to VC.
 *   Zeroing leftSideMenueOpen + rightSideMenueOpen tells cluster no flap
 *   is open, hiding them visually.
 *
 * Patched behavior (if class-replacement works):
 *   - While CarPlay active: force both flap bits to false.
 *   - When inactive: pass-through unchanged.
 *
 * All other methods bit-identical to decompiled MU1316 stock.
 */
package de.audi.app.combi.bap.app.navi;

import de.audi.app.bap.fw.AbstractBAPModuleInitializationManagerFSG;
import de.audi.app.bap.fw.functionsync.IFunctionSynchronizationHandler;
import de.audi.app.bap.fw.functiontypes.BAPFunctionArrayFSG;
import de.audi.app.bap.fw.functiontypes.BAPFunctionMethodFSG;
import de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG;
import de.audi.app.bap.utils.BAPStringUtilities;
import de.audi.app.combi.bap.AbstractCombiConnector;
import de.audi.app.combi.bap.app.navi.list.AddressListArrayHandler;
import de.audi.app.combi.bap.app.navi.list.FavoriteDestinationsListHandler;
import de.audi.app.combi.bap.app.navi.list.LaneGuidanceHandler;
import de.audi.app.combi.bap.app.navi.list.LastDestinationsListHandler;
import de.audi.app.combi.bap.utils.CombiLogger;
import de.audi.atip.interapp.bap.BAPServiceListener;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPTMCInfoMessage;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationInfo;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationListEntry;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviDestination;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPSemiDynamicRouteInfo;
import de.audi.atip.interapp.combi.bap.navi.data.EtcStatus;
import de.audi.atip.log.LogChannel;
import de.vw.mib.bap.generated.navsd.serializer.ActiveRgType_Status;
import de.vw.mib.bap.generated.navsd.serializer.Altitude_Status;
import de.vw.mib.bap.generated.navsd.serializer.CompassInfo_Status;
import de.vw.mib.bap.generated.navsd.serializer.CurrentPositionInfo_Status;
import de.vw.mib.bap.generated.navsd.serializer.DestinationInfo_Status;
import de.vw.mib.bap.generated.navsd.serializer.DistanceToDestination_Status;
import de.vw.mib.bap.generated.navsd.serializer.DistanceToNextManeuver_Status;
import de.vw.mib.bap.generated.navsd.serializer.ETC_Status_Status;
import de.vw.mib.bap.generated.navsd.serializer.Exitview_Status;
import de.vw.mib.bap.generated.navsd.serializer.FSG_Setup_Status;
import de.vw.mib.bap.generated.navsd.serializer.InfoStates_Status;
import de.vw.mib.bap.generated.navsd.serializer.ManeuverDescriptor_Status;
import de.vw.mib.bap.generated.navsd.serializer.ManeuverState_Status;
import de.vw.mib.bap.generated.navsd.serializer.MapColorAndType_Status;
import de.vw.mib.bap.generated.navsd.serializer.MapScale_Status;
import de.vw.mib.bap.generated.navsd.serializer.MapViewAndOrientation_Status;
import de.vw.mib.bap.generated.navsd.serializer.Map_Presentation_Status;
import de.vw.mib.bap.generated.navsd.serializer.OnlineNavigationState_Status;
import de.vw.mib.bap.generated.navsd.serializer.POI_Search_Result;
import de.vw.mib.bap.generated.navsd.serializer.RG_Status_Status;
import de.vw.mib.bap.generated.navsd.serializer.RepeatLastNavAnnouncement_Result;
import de.vw.mib.bap.generated.navsd.serializer.SemidynamicRouteGuidance_Status;
import de.vw.mib.bap.generated.navsd.serializer.TimeToDestination_Status;
import de.vw.mib.bap.generated.navsd.serializer.TrafficBlock_Indication_Status;
import de.vw.mib.bap.generated.navsd.serializer.TurnToInfo_Status;
import de.vw.mib.bap.generated.navsd.serializer.VoiceGuidance_Status;
import java.util.Date;
import java.util.GregorianCalendar;

public class AppConnectorNavi extends AbstractCombiConnector implements CombiBAPServiceNavi {
    private final LogChannel logChannelFrequent = CombiLogger.getFwNaviFrequentLog();
    private static final int[] COMPASS_SYMBOLIC_2_ANGLE = new int[16];

    protected AppConnectorNavi(CombiModuleNavi combiModuleNavi) {
        super(combiModuleNavi);
    }

    public void setAppServiceListener(BAPServiceListener bAPServiceListener) {
        super.setAppServiceListener(bAPServiceListener);
        this.moduleFsg.getInitializationManager().notifyAppServiceChanged(bAPServiceListener != null);
    }

    public void showInitializingScreen() {
        this.logChannel.log(10000000, "[AppConnectorNavi#showInitializingScreen]");
        ((AbstractBAPModuleInitializationManagerFSG)this.moduleFsg.getInitializationManager()).setAppIsReady(false);
    }

    public void hideInitializingScreen() {
        this.logChannel.log(1000000, "[AppConnectorNavi#hideInitializingScreen]");
        ((AbstractBAPModuleInitializationManagerFSG)this.moduleFsg.getInitializationManager()).setAppIsReady(true);
    }

    public void updateCompassInfo(int i, int j) {
        this.logChannel
            .log(1000000, "[AppConnectorNavi#updateCompassInfo] called (directionAngle=%1, directionSymbolic=%2", i, j);
        CompassInfo_Status compassInfo_Status = new CompassInfo_Status();
        if (i >= 0 && i <= 360) {
            compassInfo_Status.direction_Angle = i;
        } else {
            compassInfo_Status.direction_Angle = this.convertDirectionSymbolicToAngle(j);
        }

        if (j == 255) {
            compassInfo_Status.direction_Symbolic = this.convertDirectionAngleToSymbolic(i);
        } else {
            compassInfo_Status.direction_Symbolic = j;
        }

        this.moduleFsg.getBAPFunctionPropertyFSG(16).sendStatusIfChanged(compassInfo_Status);
    }

    private int convertDirectionSymbolicToAngle(int i) {
        int j;
        if (i >= 0 && i < COMPASS_SYMBOLIC_2_ANGLE.length) {
            j = COMPASS_SYMBOLIC_2_ANGLE[i];
        } else {
            j = 65535;
        }

        this.logChannel
            .log(10000000, "[AppConnectorNavi#convertDirectionSymbolicToAngle] symbolic=%1 -> angle=%2", i, j);
        return j;
    }

    private int convertDirectionAngleToSymbolic(int i) {
        int j = (int)((i + 11.25F) % 360.0F / 22.5F);
        short s;
        switch (j) {
            case 0:
                s = 0;
                break;
            case 1:
                s = 15;
                break;
            case 2:
                s = 14;
                break;
            case 3:
                s = 13;
                break;
            case 4:
                s = 12;
                break;
            case 5:
                s = 11;
                break;
            case 6:
                s = 10;
                break;
            case 7:
                s = 9;
                break;
            case 8:
                s = 8;
                break;
            case 9:
                s = 7;
                break;
            case 10:
                s = 6;
                break;
            case 11:
                s = 5;
                break;
            case 12:
                s = 4;
                break;
            case 13:
                s = 3;
                break;
            case 14:
                s = 2;
                break;
            case 15:
                s = 1;
                break;
            default:
                s = 255;
        }

        this.logChannel
            .log(10000000, "[AppConnectorNavi#convertDirectionAngleToSymbolic] angle=%1 -> symbolic=%2", i, s);
        return s;
    }

    public void updateRGStatus(int i) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateRGStatus] called (rgStatus=%1)", (long)i);
        RG_Status_Status rG_Status_Status = (RG_Status_Status)this.moduleFsg
            .getBAPFunctionPropertyFSG(17)
            .getLastStatus();
        if (rG_Status_Status.rg_Status != i) {
            this.logChannel.log(1000000, "[AppConnectorNavi#updateRGStatus] changed -> trigger FctSync");
            IFunctionSynchronizationHandler iFunctionSynchronizationHandler = this.moduleFsg
                .getFunctionSynchronizationHandler();
            iFunctionSynchronizationHandler.startSync(0);
        }

        RG_Status_Status rG_Status_Status1 = new RG_Status_Status();
        rG_Status_Status1.rg_Status = i;
        this.moduleFsg.getBAPFunctionPropertyFSG(17).sendStatusIfChanged(rG_Status_Status1);
    }

    public void updateActiveRGType(int i) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateActiveRGType] called (rgType=%1)", (long)i);
        ActiveRgType_Status activeRgType_Status = new ActiveRgType_Status();
        activeRgType_Status.rgtype = i;
        this.moduleFsg.getBAPFunctionPropertyFSG(39).sendStatusIfChanged(activeRgType_Status);
    }

    public void updateDistanceToNextManeuver(int i, int j, boolean bl, int k) {
        this.logChannelFrequent
            .log(10000000, "[AppConnectorNavi#updateDistanceToNextManeuver] called (distance=%1, unit=%2, ...", i, j);
        this.logChannelFrequent
            .log(10000000, "[AppConnectorNavi#updateDistanceToNextManeuver] ... BGEnabled=%1, BGValue=%2)", bl, k);
        DistanceToNextManeuver_Status distanceToNextManeuver_Status = new DistanceToNextManeuver_Status();
        int l = bl ? 1 : 0;
        if (i == -1) {
            distanceToNextManeuver_Status.distanceToNextManeuver.distance = 0;
            distanceToNextManeuver_Status.distanceToNextManeuver.unit = 255;
            distanceToNextManeuver_Status.validityInformation.distanceToNextManeuverValid = false;
        } else {
            distanceToNextManeuver_Status.distanceToNextManeuver.distance = i;
            distanceToNextManeuver_Status.distanceToNextManeuver.unit = j;
            distanceToNextManeuver_Status.validityInformation.distanceToNextManeuverValid = true;
        }

        distanceToNextManeuver_Status.bargraphInfo.bargraphOnOff = l;
        distanceToNextManeuver_Status.bargraphInfo.bargraph = k;
        this.moduleFsg.getBAPFunctionPropertyFSG(18).sendStatusIfChanged(distanceToNextManeuver_Status);
    }

    public void updateCurrentPositionInfo(String string) {
        this.logChannel
            .log(10000000, "[AppConnectorNavi#updateCurrentPositionInfo] called (currentPositionInfo=%1)", string);
        CurrentPositionInfo_Status currentPositionInfo_Status = new CurrentPositionInfo_Status();
        currentPositionInfo_Status.positionInfo.setContent(string);
        this.moduleFsg.getBAPFunctionPropertyFSG(19).sendStatusIfChanged(currentPositionInfo_Status);
    }

    public void updateTurnToInfo(String string, String string1) {
        this.logChannel
            .log(1000000, "[AppConnectorNavi#updateTurnToInfo] called (turnToInfo=%1, signPost=%2)", string, string1);
        TurnToInfo_Status turnToInfo_Status = new TurnToInfo_Status();
        turnToInfo_Status.turnToInfo.setContent(string);
        turnToInfo_Status.signPost.setContent(string1);
        this.moduleFsg.getBAPFunctionPropertyFSG(20).sendStatusIfChanged(turnToInfo_Status);
    }

    public void updateDistanceToDestination(int i, int j, boolean bl) {
        this.logChannelFrequent
            .log(
                10000000,
                "[AppConnectorNavi#updateDistanceToDestination] called (distance=%1, unit=%2, isDistanceToStopOver=%3)",
                i,
                j,
                bl
            );
        DistanceToDestination_Status distanceToDestination_Status = new DistanceToDestination_Status();
        if (i == -1) {
            distanceToDestination_Status.distanceToDestination.distance = 0;
            distanceToDestination_Status.distanceToDestination.unit = 255;
            distanceToDestination_Status.distanceToDestinationType.distanceToStopover = false;
            distanceToDestination_Status.validityInformation.distanceToDestinationValid = false;
        } else {
            distanceToDestination_Status.distanceToDestination.distance = i;
            distanceToDestination_Status.distanceToDestination.unit = j;
            distanceToDestination_Status.distanceToDestinationType.distanceToStopover = bl;
            distanceToDestination_Status.validityInformation.distanceToDestinationValid = true;
        }

        this.moduleFsg.getBAPFunctionPropertyFSG(21).sendStatusIfChanged(distanceToDestination_Status);
    }

    public void updateTimeToDestination(int i, int j, long l) {
        this.logChannelFrequent
            .log(
                10000000,
                "[AppConnectorNavi#updateTimeToDestination] called (timeInfoType=%1, navigationTimeFormat=%2, ...",
                i,
                j
            );
        this.logChannelFrequent.log(10000000, "[AppConnectorNavi#updateTimeToDestination] ... time=%1)", l);
        TimeToDestination_Status timeToDestination_Status = new TimeToDestination_Status();
        timeToDestination_Status.timeInfo.timeInfoType = i;
        timeToDestination_Status.timeInfo.navigationTimeFormat = j;
        boolean bl = l >= 0L;
        if (bl) {
            boolean bl1 = i == 1;
            if (bl1) {
                Date date = new Date(l * 1000L);
                GregorianCalendar gregorianCalendar = new GregorianCalendar();
                gregorianCalendar.setTime(date);
                timeToDestination_Status.timeInfo.year = gregorianCalendar.get(1) % 100;
                timeToDestination_Status.timeInfo.month = gregorianCalendar.get(2) + 1;
                timeToDestination_Status.timeInfo.day = gregorianCalendar.get(5);
                timeToDestination_Status.timeInfo.hour = gregorianCalendar.get(11);
                timeToDestination_Status.timeInfo.minute = gregorianCalendar.get(12);
            } else {
                int k = (int)(l / 60L);
                timeToDestination_Status.timeInfo.hour = k / 60;
                timeToDestination_Status.timeInfo.minute = k % 60;
            }

            timeToDestination_Status.validityInformation.timeInfo_YearIsAvailableToBeDisplayed = bl1;
            timeToDestination_Status.validityInformation.timeInfo_MonthIsAvailableToBeDisplayed = bl1;
            timeToDestination_Status.validityInformation.timeInfo_DayIsAvailableToBeDisplayed = bl1;
            timeToDestination_Status.validityInformation.timeInfo_HourIsValid = true;
            timeToDestination_Status.validityInformation.timeInfo_MinuteIsValid = true;
        } else {
            timeToDestination_Status.timeInfo.year = 0;
            timeToDestination_Status.timeInfo.month = 0;
            timeToDestination_Status.timeInfo.day = 0;
            timeToDestination_Status.timeInfo.hour = 0;
            timeToDestination_Status.timeInfo.minute = 0;
            timeToDestination_Status.validityInformation.timeInfo_YearIsAvailableToBeDisplayed = false;
            timeToDestination_Status.validityInformation.timeInfo_MonthIsAvailableToBeDisplayed = false;
            timeToDestination_Status.validityInformation.timeInfo_DayIsAvailableToBeDisplayed = false;
            timeToDestination_Status.validityInformation.timeInfo_HourIsValid = false;
            timeToDestination_Status.validityInformation.timeInfo_MinuteIsValid = false;
        }

        this.moduleFsg.getBAPFunctionPropertyFSG(22).sendStatusIfChanged(timeToDestination_Status);
    }

    public void updateManeuverDescriptor(CombiBAPNaviManeuverDescriptor[] combiBAPNaviManeuverDescriptors) {
        this.logChannel.log(10000000, "[AppConnectorNavi#updateManeuverDescriptor] called");
        if (combiBAPNaviManeuverDescriptors == null) {
            this.logChannel.log(10000, "[AppConnectorNavi#updateManeuverDescriptor] maneuver descriptor is null");
        } else {
            ManeuverDescriptor_Status maneuverDescriptor_Status = this.createManeuverDescriptorStatus(
                combiBAPNaviManeuverDescriptors
            );
            if (maneuverDescriptorNeedsToBeSynchronized(maneuverDescriptor_Status)) {
                IFunctionSynchronizationHandler iFunctionSynchronizationHandler = this.moduleFsg
                    .getFunctionSynchronizationHandler();
                if (!iFunctionSynchronizationHandler.isSyncActive() && !iFunctionSynchronizationHandler.isSyncPending()
                    )
                 {
                    iFunctionSynchronizationHandler.startSync(1);
                }
            }

            this.moduleFsg.getBAPFunctionPropertyFSG(23).sendStatusIfChanged(maneuverDescriptor_Status);
        }
    }

    private static boolean maneuverDescriptorNeedsToBeSynchronized(ManeuverDescriptor_Status maneuverDescriptor_Status) {
        return maneuverDescriptor_Status.maneuver_1.mainElement != 6
            && maneuverDescriptor_Status.maneuver_1.mainElement != 9
            && maneuverDescriptor_Status.maneuver_1.mainElement != 10;
    }

    private ManeuverDescriptor_Status createManeuverDescriptorStatus(
        CombiBAPNaviManeuverDescriptor[] combiBAPNaviManeuverDescriptors
    ) {
        ManeuverDescriptor_Status maneuverDescriptor_Status = new ManeuverDescriptor_Status();
        if (combiBAPNaviManeuverDescriptors.length > 0 && combiBAPNaviManeuverDescriptors[0] != null) {
            this.logChannel
                .log(
                    10000000,
                    "[AppConnectorNavi#createManeuverDescriptorStatus] maneuver 1 = %1",
                    combiBAPNaviManeuverDescriptors[0]
                );
            maneuverDescriptor_Status.maneuver_1.mainElement = getMappedManeuverMainElement(
                combiBAPNaviManeuverDescriptors[0].mainElement, combiBAPNaviManeuverDescriptors[0].direction
            );
            maneuverDescriptor_Status.maneuver_1.direction = combiBAPNaviManeuverDescriptors[0].direction;
            maneuverDescriptor_Status.maneuver_1.zLevelGuidance = combiBAPNaviManeuverDescriptors[0].zLevelGuidance;
            maneuverDescriptor_Status.maneuver_1
                .sidestreets
                .setContent(BAPStringUtilities.convertToRawString(combiBAPNaviManeuverDescriptors[0].sideStreets));
        } else {
            this.logChannel.log(10000000, "[AppConnectorNavi#createManeuverDescriptorStatus] reset maneuver 1");
            maneuverDescriptor_Status.maneuver_1.mainElement = 0;
            maneuverDescriptor_Status.maneuver_1.direction = 0;
            maneuverDescriptor_Status.maneuver_1.zLevelGuidance = 0;
            maneuverDescriptor_Status.maneuver_1.sidestreets.setContent("isNativeLittleEndian");
        }

        if (combiBAPNaviManeuverDescriptors.length > 1 && combiBAPNaviManeuverDescriptors[1] != null) {
            this.logChannel
                .log(
                    10000000,
                    "[AppConnectorNavi#createManeuverDescriptorStatus] maneuver 2 = %1",
                    combiBAPNaviManeuverDescriptors[1]
                );
            maneuverDescriptor_Status.maneuver_2.mainElement = getMappedManeuverMainElement(
                combiBAPNaviManeuverDescriptors[1].mainElement, combiBAPNaviManeuverDescriptors[1].direction
            );
            maneuverDescriptor_Status.maneuver_2.direction = combiBAPNaviManeuverDescriptors[1].direction;
            maneuverDescriptor_Status.maneuver_2.zLevelGuidance = combiBAPNaviManeuverDescriptors[1].zLevelGuidance;
            maneuverDescriptor_Status.maneuver_2
                .sidestreets
                .setContent(BAPStringUtilities.convertToRawString(combiBAPNaviManeuverDescriptors[1].sideStreets));
        } else {
            this.logChannel.log(10000000, "[AppConnectorNavi#createManeuverDescriptorStatus] reset maneuver 2");
            maneuverDescriptor_Status.maneuver_2.mainElement = 0;
            maneuverDescriptor_Status.maneuver_2.direction = 0;
            maneuverDescriptor_Status.maneuver_2.zLevelGuidance = 0;
            maneuverDescriptor_Status.maneuver_2.sidestreets.setContent("isNativeLittleEndian");
        }

        if (combiBAPNaviManeuverDescriptors.length > 2 && combiBAPNaviManeuverDescriptors[2] != null) {
            this.logChannel
                .log(
                    10000000,
                    "[AppConnectorNavi#createManeuverDescriptorStatus] maneuver 3 = %1",
                    combiBAPNaviManeuverDescriptors[2]
                );
            maneuverDescriptor_Status.maneuver_3.mainElement = getMappedManeuverMainElement(
                combiBAPNaviManeuverDescriptors[2].mainElement, combiBAPNaviManeuverDescriptors[2].direction
            );
            maneuverDescriptor_Status.maneuver_3.direction = combiBAPNaviManeuverDescriptors[2].direction;
            maneuverDescriptor_Status.maneuver_3.zLevelGuidance = combiBAPNaviManeuverDescriptors[2].zLevelGuidance;
            maneuverDescriptor_Status.maneuver_3
                .sidestreets
                .setContent(BAPStringUtilities.convertToRawString(combiBAPNaviManeuverDescriptors[2].sideStreets));
        } else {
            this.logChannel.log(10000000, "[AppConnectorNavi#createManeuverDescriptorStatus] reset maneuver 3");
            maneuverDescriptor_Status.maneuver_3.mainElement = 0;
            maneuverDescriptor_Status.maneuver_3.direction = 0;
            maneuverDescriptor_Status.maneuver_3.zLevelGuidance = 0;
            maneuverDescriptor_Status.maneuver_3.sidestreets.setContent("isNativeLittleEndian");
        }

        return maneuverDescriptor_Status;
    }

    private static int getMappedManeuverMainElement(int i, int j) {
        int k = i;
        if (i == 28 && (j == 64 || j == 192)) {
            k = 12;
        }

        return k;
    }

    public void updateLaneGuidance(boolean bl, CombiBAPNaviLaneGuidanceData[] combiBAPNaviLaneGuidanceDatas) {
        this.logChannel
            .log(
                1000000,
                "[AppConnectorNavi#updateLaneGuidance] called (enableLaneGuidance=%1, dataSize=%2)",
                bl,
                combiBAPNaviLaneGuidanceDatas == null ? 0L : combiBAPNaviLaneGuidanceDatas.length
            );
        BAPFunctionArrayFSG bAPFunctionArrayFSG = this.moduleFsg.getBAPFunctionArrayFSG(24);
        LaneGuidanceHandler laneGuidanceHandler = (LaneGuidanceHandler)bAPFunctionArrayFSG.getArrayHandler();
        if (laneGuidanceHandler != null) {
            laneGuidanceHandler.setLaneGuidanceEnabled(bl);
            laneGuidanceHandler.updateList(combiBAPNaviLaneGuidanceDatas);
        }
    }

    public void updateTMCInfoMessages(CombiBAPTMCInfoMessage[] combiBAPTMCInfoMessages) {
        this.logChannel
            .log(
                1000000,
                "[AppConnectorNavi#updateTMCInfoMessages] called (noOfMessages=%1)",
                combiBAPTMCInfoMessages == null ? 0L : combiBAPTMCInfoMessages.length
            );
        ((CombiModuleNavi)this.moduleFsg).getTMCInfoHandler().updateTMCInfoMessages(combiBAPTMCInfoMessages);
    }

    public void routeGuidanceActDeactResult(int i) {
        this.logChannel.log(1000000, "[AppConnectorNavi#routeGuidanceActDeactResult] called (result=%1)", (long)i);
        ((CombiModuleNavi)this.moduleFsg).rgActDeactResultReceived(i);
    }

    public void repeatLastNavAnnouncementResult(int i) {
        this.logChannel.log(1000000, "[AppConnectorNavi#repeatLastNavAnnouncementResult] called (result=%1)", (long)i);
        BAPFunctionMethodFSG bAPFunctionMethodFSG = this.moduleFsg.getBAPFunctionMethodFSG(35);
        RepeatLastNavAnnouncement_Result repeatLastNavAnnouncement_Result = (RepeatLastNavAnnouncement_Result)this.moduleFsg
            .createResultSerializer(35);
        repeatLastNavAnnouncement_Result.repeatLna_Result = i;
        bAPFunctionMethodFSG.resultREQ(repeatLastNavAnnouncement_Result);
    }

    public void updateVoiceGuidanceState(int i) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateVoiceGuidanceState] called (state=%1)", (long)i);
        VoiceGuidance_Status voiceGuidance_Status = new VoiceGuidance_Status();
        voiceGuidance_Status.voiceGuidance_State = i;
        this.moduleFsg.getBAPFunctionPropertyFSG(36).sendStatusIfChanged(voiceGuidance_Status);
    }

    public void updateInfoStates(int i) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateInfoStates] called (states=%1)", (long)i);
        InfoStates_Status infoStates_Status = new InfoStates_Status();
        infoStates_Status.states = i;
        this.moduleFsg.getBAPFunctionPropertyFSG(38).sendStatusIfChanged(infoStates_Status);
    }

    public void updateTrafficBlockIndication(int i) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateTrafficBlockIndication] called (tmcSymbol=%1)", (long)i);
        TrafficBlock_Indication_Status trafficBlock_Indication_Status = new TrafficBlock_Indication_Status();
        trafficBlock_Indication_Status.tmc_Symbol = i;
        this.moduleFsg.getBAPFunctionPropertyFSG(40).sendStatusIfChanged(trafficBlock_Indication_Status);
    }

    public void updateLastDestinationsList(CombiBAPDestinationListEntry[] combiBAPDestinationListEntrys) {
        this.logChannel
            .log(
                1000000,
                "[AppConnectorNavi#updateLastDestinationsList] called (dataSize=%1)",
                combiBAPDestinationListEntrys == null ? 0L : combiBAPDestinationListEntrys.length
            );
        BAPFunctionArrayFSG bAPFunctionArrayFSG = this.moduleFsg.getBAPFunctionArrayFSG(29);
        LastDestinationsListHandler lastDestinationsListHandler = (LastDestinationsListHandler)bAPFunctionArrayFSG.getArrayHandler();
        if (lastDestinationsListHandler != null) {
            lastDestinationsListHandler.updateList(combiBAPDestinationListEntrys);
        } else {
            this.logChannel
                .log(
                    10000,
                    "[AppConnectorNavi#updateFavoriteDestinationsList] no array handler registered for BAP_FCT_ID_LAST_DEST_LIST"
                );
        }
    }

    public void updateFavoriteDestinationsList(CombiBAPDestinationListEntry[] combiBAPDestinationListEntrys) {
        this.logChannel
            .log(
                1000000,
                "[AppConnectorNavi#updateFavoriteDestinationsList] called (dataSize=%1)",
                combiBAPDestinationListEntrys == null ? 0L : combiBAPDestinationListEntrys.length
            );
        BAPFunctionArrayFSG bAPFunctionArrayFSG = this.moduleFsg.getBAPFunctionArrayFSG(30);
        FavoriteDestinationsListHandler favoriteDestinationsListHandler = (FavoriteDestinationsListHandler)bAPFunctionArrayFSG.getArrayHandler();
        if (favoriteDestinationsListHandler != null) {
            favoriteDestinationsListHandler.updateList(combiBAPDestinationListEntrys);
        } else {
            this.logChannel
                .log(
                    10000,
                    "[AppConnectorNavi#updateFavoriteDestinationsList] no array handler registered for BAP_FCT_ID_FAVORITE_DEST_LIST"
                );
        }
    }

    public void updateHomeAddress(CombiBAPNaviDestination combiBAPNaviDestination) {
        this.logChannel
            .log(
                1000000,
                "[AppConnectorNavi#updateHomeAddress] homeAddress=%1 ",
                combiBAPNaviDestination != null ? combiBAPNaviDestination.toString() : "null!"
            );
        AddressListArrayHandler addressListArrayHandler = (AddressListArrayHandler)this.moduleFsg
            .getBAPFunctionArrayFSG(33)
            .getArrayHandler();
        addressListArrayHandler.updateHomeAddress(combiBAPNaviDestination);
    }

    public void updateMapColor(int i) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapColor] color=%1", (long)i);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(43);
        MapColorAndType_Status mapColorAndType_Status = (MapColorAndType_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapColorAndType_Status mapColorAndType_Status1 = new MapColorAndType_Status();
        mapColorAndType_Status1.activeMapType = mapColorAndType_Status.activeMapType;
        mapColorAndType_Status1.mainMapSetup = mapColorAndType_Status.mainMapSetup;
        mapColorAndType_Status1.supportedMapTypes.rangeMapIsSupported = mapColorAndType_Status.supportedMapTypes.rangeMapIsSupported;
        mapColorAndType_Status1.supportedMapTypes.overviewMapIsSupported = mapColorAndType_Status.supportedMapTypes.overviewMapIsSupported;
        mapColorAndType_Status1.supportedMapTypes.position3DMapIsSupported = mapColorAndType_Status.supportedMapTypes.position3DMapIsSupported;
        mapColorAndType_Status1.supportedMapTypes.position2DMapIsSupported = mapColorAndType_Status.supportedMapTypes.position2DMapIsSupported;
        mapColorAndType_Status1.supportedMapTypes.destinationMapIsSupported = mapColorAndType_Status.supportedMapTypes.destinationMapIsSupported;
        mapColorAndType_Status1.supportedMapTypes.mainMapSetupIsSupported = mapColorAndType_Status.supportedMapTypes.mainMapSetupIsSupported;
        mapColorAndType_Status1.colour = i;
        bAPFunctionPropertyFSG.sendStatusIfChanged(mapColorAndType_Status1);
    }

    public void updateMapType(int i, int j) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapType] activeMapType=%1, mainMapSetup=%2", i, j);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(43);
        MapColorAndType_Status mapColorAndType_Status = (MapColorAndType_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapColorAndType_Status mapColorAndType_Status1 = new MapColorAndType_Status();
        if (this.logChannel.isInfo() && mapColorAndType_Status.mainMapSetup != mapColorAndType_Status1.mainMapSetup) {
            String string;
            switch (mapColorAndType_Status1.mainMapSetup) {
                case 0:
                    string = "NO MAP IN ASG";
                    break;
                case 1:
                    string = "MAIN MAP IN ASG";
                    break;
                case 2:
                    string = "MAIN MAP IN FSG";
                    break;
                default:
                    string = "NOT SUPPORTED OR INVALID VALUE";
            }

            this.logChannel.log(1000000, "[AppConnectorNavi#updateMapType] main map setup changed: %1", string);
        }

        mapColorAndType_Status1.colour = mapColorAndType_Status.colour;
        mapColorAndType_Status1.supportedMapTypes.rangeMapIsSupported = mapColorAndType_Status.supportedMapTypes.rangeMapIsSupported;
        mapColorAndType_Status1.supportedMapTypes.overviewMapIsSupported = mapColorAndType_Status.supportedMapTypes.overviewMapIsSupported;
        mapColorAndType_Status1.supportedMapTypes.position3DMapIsSupported = mapColorAndType_Status.supportedMapTypes.position3DMapIsSupported;
        mapColorAndType_Status1.supportedMapTypes.position2DMapIsSupported = mapColorAndType_Status.supportedMapTypes.position2DMapIsSupported;
        mapColorAndType_Status1.supportedMapTypes.destinationMapIsSupported = mapColorAndType_Status.supportedMapTypes.destinationMapIsSupported;
        mapColorAndType_Status1.supportedMapTypes.mainMapSetupIsSupported = mapColorAndType_Status.supportedMapTypes.mainMapSetupIsSupported;
        mapColorAndType_Status1.activeMapType = i;
        mapColorAndType_Status1.mainMapSetup = j;
        bAPFunctionPropertyFSG.sendStatusIfChanged(mapColorAndType_Status1);
    }

    public void updateSupportedMapTypes(boolean bl, int i) {
        this.logChannel
            .log(1000000, "[AppConnectorNavi#updateSupportedMapTypes] mainMapSupported=%1, supportedMapTypes=%2", bl, i);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(43);
        MapColorAndType_Status mapColorAndType_Status = (MapColorAndType_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapColorAndType_Status mapColorAndType_Status1 = new MapColorAndType_Status();
        mapColorAndType_Status1.colour = mapColorAndType_Status.colour;
        mapColorAndType_Status1.activeMapType = mapColorAndType_Status.activeMapType;
        mapColorAndType_Status1.mainMapSetup = mapColorAndType_Status.mainMapSetup;
        mapColorAndType_Status1.supportedMapTypes.rangeMapIsSupported = false;
        mapColorAndType_Status1.supportedMapTypes.overviewMapIsSupported = this.hasAttribute(i, 8);
        mapColorAndType_Status1.supportedMapTypes.position3DMapIsSupported = this.hasAttribute(i, 4);
        mapColorAndType_Status1.supportedMapTypes.position2DMapIsSupported = this.hasAttribute(i, 2);
        mapColorAndType_Status1.supportedMapTypes.destinationMapIsSupported = this.hasAttribute(i, 1);
        mapColorAndType_Status1.supportedMapTypes.mainMapSetupIsSupported = bl;
        bAPFunctionPropertyFSG.sendStatusIfChanged(mapColorAndType_Status1);
    }

    public void updateMapView(int i, int j) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapView] mapView=%1, supplementaryMapView=%2", i, j);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(44);
        MapViewAndOrientation_Status mapViewAndOrientation_Status = (MapViewAndOrientation_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapViewAndOrientation_Status mapViewAndOrientation_Status1 = cloneMapViewAndOrientationStatus(
            mapViewAndOrientation_Status
        );
        mapViewAndOrientation_Status1.mapView = i;
        mapViewAndOrientation_Status1.supplementaryMapView = j;
        bAPFunctionPropertyFSG.sendStatusIfChanged(mapViewAndOrientation_Status1);
    }

    public void updateSupportedMapViews(int i, int j) {
        this.logChannel
            .log(
                1000000,
                "[AppConnectorNavi#updateSupportedMapViews] supportedMapViews=%1, supportedSupplementaryMapViews=%2",
                i,
                j
            );
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(44);
        MapViewAndOrientation_Status mapViewAndOrientation_Status = (MapViewAndOrientation_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapViewAndOrientation_Status mapViewAndOrientation_Status1 = cloneMapViewAndOrientationStatus(
            mapViewAndOrientation_Status
        );
        mapViewAndOrientation_Status1.supportedMapViews.trafficMapIsSupported = this.hasAttribute(i, 4);
        mapViewAndOrientation_Status1.supportedMapViews.googleEarthMapIsSupported = this.hasAttribute(i, 2);
        mapViewAndOrientation_Status1.supportedMapViews.standardMapIsSupported = this.hasAttribute(i, 1);
        mapViewAndOrientation_Status1.supportedSupplementaryMapViews._1BoxSupported = this.hasAttribute(j, 4);
        mapViewAndOrientation_Status1.supportedSupplementaryMapViews.compassSupported = this.hasAttribute(j, 2);
        mapViewAndOrientation_Status1.supportedSupplementaryMapViews.intersectionZoomSupported = this.hasAttribute(j, 1);
        bAPFunctionPropertyFSG.sendStatusIfChanged(mapViewAndOrientation_Status1);
    }

    public void updateMapVisibility(boolean bl, boolean bl1) {
        this.logChannel
            .log(
                1000000,
                "[AppConnectorNavi#updateMapVisibility] lvdsMapVisible=%1, supplementaryMapViewVisible=%2",
                bl,
                bl1
            );
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(44);
        MapViewAndOrientation_Status mapViewAndOrientation_Status = (MapViewAndOrientation_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapViewAndOrientation_Status mapViewAndOrientation_Status1 = cloneMapViewAndOrientationStatus(
            mapViewAndOrientation_Status
        );
        mapViewAndOrientation_Status1.mapVisibility.lvdsMapIsVisible = bl;
        mapViewAndOrientation_Status1.mapVisibility.supplementaryMapViewIsVisible = bl1;
        bAPFunctionPropertyFSG.sendStatusIfChanged(mapViewAndOrientation_Status1);
    }

    public void updateMapOrientation(int i) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapOrientation] orientation=%1", (long)i);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(44);
        MapViewAndOrientation_Status mapViewAndOrientation_Status = (MapViewAndOrientation_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapViewAndOrientation_Status mapViewAndOrientation_Status1 = cloneMapViewAndOrientationStatus(
            mapViewAndOrientation_Status
        );
        mapViewAndOrientation_Status1.mapOrientation = i;
        bAPFunctionPropertyFSG.sendStatusIfChanged(mapViewAndOrientation_Status1);
    }

    private static MapViewAndOrientation_Status cloneMapViewAndOrientationStatus(
        MapViewAndOrientation_Status mapViewAndOrientation_Status
    ) {
        MapViewAndOrientation_Status mapViewAndOrientation_Status1 = new MapViewAndOrientation_Status();
        mapViewAndOrientation_Status1.supportedMapViews.trafficMapIsSupported = mapViewAndOrientation_Status.supportedMapViews
            .trafficMapIsSupported;
        mapViewAndOrientation_Status1.supportedMapViews.googleEarthMapIsSupported = mapViewAndOrientation_Status.supportedMapViews
            .googleEarthMapIsSupported;
        mapViewAndOrientation_Status1.supportedMapViews.standardMapIsSupported = mapViewAndOrientation_Status.supportedMapViews
            .standardMapIsSupported;
        mapViewAndOrientation_Status1.supportedSupplementaryMapViews._1BoxSupported = mapViewAndOrientation_Status.supportedSupplementaryMapViews
            ._1BoxSupported;
        mapViewAndOrientation_Status1.supportedSupplementaryMapViews.compassSupported = mapViewAndOrientation_Status.supportedSupplementaryMapViews
            .compassSupported;
        mapViewAndOrientation_Status1.supportedSupplementaryMapViews.intersectionZoomSupported = mapViewAndOrientation_Status.supportedSupplementaryMapViews
            .intersectionZoomSupported;
        mapViewAndOrientation_Status1.mapVisibility.supplementaryMapViewIsVisible = mapViewAndOrientation_Status.mapVisibility
            .supplementaryMapViewIsVisible;
        mapViewAndOrientation_Status1.mapVisibility.lvdsMapIsVisible = mapViewAndOrientation_Status.mapVisibility.lvdsMapIsVisible;
        mapViewAndOrientation_Status1.modification.googleMapCanNotBeModified = mapViewAndOrientation_Status.modification
            .googleMapCanNotBeModified;
        mapViewAndOrientation_Status1.mapView = mapViewAndOrientation_Status.mapView;
        mapViewAndOrientation_Status1.supplementaryMapView = mapViewAndOrientation_Status.supplementaryMapView;
        mapViewAndOrientation_Status1.mapOrientation = mapViewAndOrientation_Status.mapOrientation;
        return mapViewAndOrientation_Status1;
    }

    public void updateMapScale(int i, boolean bl, int j, int k, boolean bl1) {
        this.logChannel
            .log(
                1000000,
                "[AppConnectorNavi#updateMapScale] autoZoomSetting=%1, autoZoomEnabled=%3, zoomLevel=%2",
                i,
                j,
                bl
            );
        this.logChannel
            .log(1000000, "[AppConnectorNavi#updateMapScale] unit=%2, intersectionAutoZoomSupported=%1", bl1, k);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(45);
        MapScale_Status mapScale_Status = new MapScale_Status();
        mapScale_Status.autoZoom = i;
        mapScale_Status.autoZoomState.autoZoomActive = bl;
        mapScale_Status.scale = j;
        mapScale_Status.unit = k;
        mapScale_Status.supportedAutoZoom.autozoomOnForIntersectionSupported = bl1;
        bAPFunctionPropertyFSG.sendStatusIfChanged(mapScale_Status);
    }

    public void updateDestinationInfo(CombiBAPDestinationInfo combiBAPDestinationInfo) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateDestinationInfo] destInfo=%1", combiBAPDestinationInfo);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(46);
        DestinationInfo_Status destinationInfo_Status = new DestinationInfo_Status();
        destinationInfo_Status.position.latitude = (int)(
            combiBAPDestinationInfo.getDestination().getLatitude() * 1000000.0F
        );
        destinationInfo_Status.position.longitude = (int)(
            combiBAPDestinationInfo.getDestination().getLongitude() * 1000000.0F
        );
        destinationInfo_Status.totalNumOfStopovers = combiBAPDestinationInfo.getNoOfStopovers();
        destinationInfo_Status.stopover_Sn = combiBAPDestinationInfo.getNoOfNextStopover();
        destinationInfo_Status.poi_Type = combiBAPDestinationInfo.getDestination().getPOIType();
        destinationInfo_Status.poi_Description.setContent(combiBAPDestinationInfo.getDestination().getPOIDescription());
        destinationInfo_Status.street.setContent(combiBAPDestinationInfo.getDestination().getStreet());
        destinationInfo_Status.town.setContent(combiBAPDestinationInfo.getDestination().getCity());
        destinationInfo_Status.state.setContent(combiBAPDestinationInfo.getDestination().getState());
        destinationInfo_Status.postalCode.setContent(combiBAPDestinationInfo.getDestination().getPostalCode());
        destinationInfo_Status.country.setContent(combiBAPDestinationInfo.getDestination().getCountry());
        bAPFunctionPropertyFSG.sendStatusIfChanged(destinationInfo_Status);
    }

    public void updateAltitude(int i, int j) {
        this.logChannelFrequent.log(1000000, "[AppConnectorNavi#updateAltitude] altitude=%1, unit=%2", i, j);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(47);
        Altitude_Status altitude_Status = new Altitude_Status();
        altitude_Status.altitude = i;
        altitude_Status.unit = j;
        bAPFunctionPropertyFSG.sendStatusIfChanged(altitude_Status);
    }

    public void updateOnlineNavigationState(int i, int j, int k) {
        this.logChannel
            .log(
                1000000,
                "[AppConnectorNavi#updateOnlineNavigationState] state=%1, bufferProgress=%2, onlineNavigationSystem=%3",
                (long)i,
                (long)j,
                (long)k
            );
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(48);
        OnlineNavigationState_Status onlineNavigationState_Status = new OnlineNavigationState_Status();
        onlineNavigationState_Status.state = i;
        onlineNavigationState_Status.progress = j;
        onlineNavigationState_Status.onlineNavigationSystem = k;
        bAPFunctionPropertyFSG.sendStatusIfChanged(onlineNavigationState_Status);
    }

    public void updateExitView(int i, int j) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateExitView] variant=%1, exitviewID=%2", i, j);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(49);
        bAPFunctionPropertyFSG.sendStatusIfChanged(createExitViewStatus(i, j));
    }

    private static Exitview_Status createExitViewStatus(int i, int j) {
        Exitview_Status exitview_Status = new Exitview_Status();
        exitview_Status.variant = i;
        exitview_Status.exitview_Id = j;
        return exitview_Status;
    }

    public void updateSemidynamicRouteGuidance(CombiBAPSemiDynamicRouteInfo combiBAPSemiDynamicRouteInfo) {
        this.logChannel
            .log(
                1000000, "[AppConnectorNavi#updateSemidynamicRouteGuidance] routeInfo=%1", combiBAPSemiDynamicRouteInfo
            );
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(50);
        SemidynamicRouteGuidance_Status semidynamicRouteGuidance_Status = new SemidynamicRouteGuidance_Status();
        semidynamicRouteGuidance_Status.trafficImpact.trafficImpactOnCurrentRoute = combiBAPSemiDynamicRouteInfo.isTrafficImpactOnCurrentRoute();
        semidynamicRouteGuidance_Status.trafficImpact.alternativeRouteAvailable = combiBAPSemiDynamicRouteInfo.isAlternativeRouteAvailable();
        semidynamicRouteGuidance_Status.delay.minute = combiBAPSemiDynamicRouteInfo.getDelayMinute();
        semidynamicRouteGuidance_Status.delay.hour = combiBAPSemiDynamicRouteInfo.getDelayHour();
        semidynamicRouteGuidance_Status.newDistanceToDestination.distance = (int)combiBAPSemiDynamicRouteInfo.getNewDTDDistance();
        semidynamicRouteGuidance_Status.newDistanceToDestination.unit = combiBAPSemiDynamicRouteInfo.getNewDTDUnit();
        semidynamicRouteGuidance_Status.newDistanceToDestination.typeOfDistance = combiBAPSemiDynamicRouteInfo.getNewDTDTypeOfDistance();
        semidynamicRouteGuidance_Status.newTimeToDestination.timeInfoType = combiBAPSemiDynamicRouteInfo.getNewTTDTimeInfoType();
        semidynamicRouteGuidance_Status.newTimeToDestination.navigationTimeFormat = combiBAPSemiDynamicRouteInfo.getNewTTDNavigationTimeFormat();
        semidynamicRouteGuidance_Status.newTimeToDestination.minute = combiBAPSemiDynamicRouteInfo.getNewTTDMinute();
        semidynamicRouteGuidance_Status.newTimeToDestination.hour = combiBAPSemiDynamicRouteInfo.getNewTTDHour();
        semidynamicRouteGuidance_Status.newTimeToDestination.day = combiBAPSemiDynamicRouteInfo.getNewTTDDay();
        semidynamicRouteGuidance_Status.newTimeToDestination.month = combiBAPSemiDynamicRouteInfo.getNewTTDMonth();
        semidynamicRouteGuidance_Status.newTimeToDestination.year = combiBAPSemiDynamicRouteInfo.getNewTTDYear();
        semidynamicRouteGuidance_Status.validityInformation.newDistanceToDestinationIsValid = combiBAPSemiDynamicRouteInfo.isNewDTDValid();
        semidynamicRouteGuidance_Status.validityInformation.newTimeToDestination_MinuteIsValid = combiBAPSemiDynamicRouteInfo.isNewTTDMinuteValid();
        semidynamicRouteGuidance_Status.validityInformation.newTimeToDestination_HourIsValid = combiBAPSemiDynamicRouteInfo.isNewTTDHourValid();
        semidynamicRouteGuidance_Status.validityInformation.newTimeToDestination_DayIsAvailableToBeDisplayed = combiBAPSemiDynamicRouteInfo.isNewTTDDayValid();
        semidynamicRouteGuidance_Status.validityInformation.newTimeToDestination_MonthIsAvailableToBeDisplayed = combiBAPSemiDynamicRouteInfo.isNewTTDMonthValid();
        semidynamicRouteGuidance_Status.validityInformation.newTimeToDestination_YearIsAvailableToBeDisplayed = combiBAPSemiDynamicRouteInfo.isNewTTDYearValid();
        bAPFunctionPropertyFSG.sendStatusIfChanged(semidynamicRouteGuidance_Status);
    }

    public void poiSearchResult(int i, int j) {
        this.logChannel.log(1000000, "[AppConnectorNavi#poiSearchResult] result=%1, amountOfFoundEntries=%2", i, j);
        BAPFunctionMethodFSG bAPFunctionMethodFSG = this.moduleFsg.getBAPFunctionMethodFSG(51);
        POI_Search_Result pOI_Search_Result = (POI_Search_Result)this.moduleFsg.createResultSerializer(51);
        pOI_Search_Result.poi_Search_Result = i;
        pOI_Search_Result.amountOfFoundEntries = j;
        bAPFunctionMethodFSG.resultREQ(pOI_Search_Result);
    }

    public void updatePOIListSize(int i) {
        this.logChannel.log(100000, "[AppConnectorNavi#updatePOIListSize] listSize=%1 - NOT IMPLEMENTED YET", (long)i);
    }

    public void updateFSGSetup(int i, boolean bl) {
        this.logChannel
            .log(1000000, "[AppConnectorNavi#updateFSGSetup] voiceGuidanceSetup=%1, poiSearchSupported", (long)i);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(53);
        FSG_Setup_Status fSG_Setup_Status = new FSG_Setup_Status();
        fSG_Setup_Status.supported_Poi_Types.fuelStationAndParkingAreaSupported = bl;
        fSG_Setup_Status.voiceGuidance.voiceGuidanceOnAvailable = this.hasAttribute(i, 1);
        fSG_Setup_Status.voiceGuidance.voiceGuidanceOnReducedAvailable = this.hasAttribute(i, 2);
        fSG_Setup_Status.voiceGuidance.voiceGuidanceOnTrafficAvailable = this.hasAttribute(i, 4);
        bAPFunctionPropertyFSG.sendStatusIfChanged(fSG_Setup_Status);
    }

    public void updateMapPresentation(boolean bl, boolean bl1, boolean bl2) {
        /* ========== PATCH: zero both flaps while CarPlay active ========== */
        boolean origLeft = bl1;
        boolean origRight = bl2;
        boolean gated = false;
        try {
            if (com.luka.carplay.CarPlayHook.isCarplayRunning()) {
                bl1 = false;
                bl2 = false;
                gated = true;
            }
        } catch (Throwable t) {
            /* CarPlayHook not yet loaded — treat as stock */
        }
        if (gated && (origLeft || origRight)) {
            com.luka.carplay.framework.Log.i(
                "AppConnectorNavi",
                "gate: large=" + bl + " HMI(L=" + origLeft + ",R=" + origRight
                    + ") -> BAP(L=false,R=false) [CarPlay active]"
            );
        } else if (origLeft || origRight) {
            com.luka.carplay.framework.Log.d(
                "AppConnectorNavi",
                "pass: large=" + bl + " L=" + origLeft + " R=" + origRight
            );
        }
        /* ================================================================ */

        this.logChannel
            .log(
                1000000,
                "[AppConnectorNavi#updateMapPresentation] largeMapView=%1, leftSideMenuOpen=%2, rightSideMenuOpen=%3",
                bl,
                bl1,
                bl2
            );
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(54);
        Map_Presentation_Status map_Presentation_Status = new Map_Presentation_Status();
        map_Presentation_Status.asg_Hmi_State.largeMapView = bl;
        map_Presentation_Status.asg_Hmi_State.leftSideMenueOpen = bl1;
        map_Presentation_Status.asg_Hmi_State.rightSideMenueOpen = bl2;
        bAPFunctionPropertyFSG.sendStatusIfChanged(map_Presentation_Status);
    }

    public void updateManeuverState(int i) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateManeuverState] state: %1", (long)i);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(55);
        ManeuverState_Status maneuverState_Status = new ManeuverState_Status();
        maneuverState_Status.state = i;
        bAPFunctionPropertyFSG.sendStatusIfChanged(maneuverState_Status);
    }

    public void updateEtcStatus(EtcStatus etcStatus) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateEtcStatus] %1", etcStatus);
        ETC_Status_Status eTC_Status_Status = new ETC_Status_Status();
        eTC_Status_Status.cardStatus = etcStatus.getCardStatus();
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(56);
        bAPFunctionPropertyFSG.sendStatusIfChanged(eTC_Status_Status);
    }

    static {
        COMPASS_SYMBOLIC_2_ANGLE[0] = 0;
        COMPASS_SYMBOLIC_2_ANGLE[1] = 337;
        COMPASS_SYMBOLIC_2_ANGLE[2] = 315;
        COMPASS_SYMBOLIC_2_ANGLE[3] = 292;
        COMPASS_SYMBOLIC_2_ANGLE[4] = 270;
        COMPASS_SYMBOLIC_2_ANGLE[5] = 247;
        COMPASS_SYMBOLIC_2_ANGLE[6] = 225;
        COMPASS_SYMBOLIC_2_ANGLE[7] = 202;
        COMPASS_SYMBOLIC_2_ANGLE[8] = 180;
        COMPASS_SYMBOLIC_2_ANGLE[9] = 157;
        COMPASS_SYMBOLIC_2_ANGLE[10] = 135;
        COMPASS_SYMBOLIC_2_ANGLE[11] = 112;
        COMPASS_SYMBOLIC_2_ANGLE[12] = 90;
        COMPASS_SYMBOLIC_2_ANGLE[13] = 67;
        COMPASS_SYMBOLIC_2_ANGLE[14] = 45;
        COMPASS_SYMBOLIC_2_ANGLE[15] = 22;
    }
}
