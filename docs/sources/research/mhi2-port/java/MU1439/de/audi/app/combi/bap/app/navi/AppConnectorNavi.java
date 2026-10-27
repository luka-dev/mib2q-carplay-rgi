/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  de.audi.app.bap.fw.AbstractBAPModuleFSG
 *  de.audi.app.bap.fw.AbstractBAPModuleInitializationManagerFSG
 *  de.audi.app.bap.fw.functionsync.IFunctionSynchronizationHandler
 *  de.audi.app.bap.fw.functiontypes.BAPFunctionArrayFSG
 *  de.audi.app.bap.fw.functiontypes.BAPFunctionMethodFSG
 *  de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG
 *  de.audi.app.combi.bap.AbstractCombiConnector
 *  de.audi.app.combi.bap.app.navi.CombiModuleNavi
 *  de.audi.app.combi.bap.app.navi.list.AddressListArrayHandler
 *  de.audi.app.combi.bap.app.navi.list.FavoriteDestinationsListHandler
 *  de.audi.app.combi.bap.app.navi.list.LaneGuidanceHandler
 *  de.audi.app.combi.bap.app.navi.list.LastDestinationsListHandler
 *  de.audi.app.combi.bap.utils.CombiLogger
 *  de.audi.atip.interapp.bap.BAPServiceListener
 *  de.audi.atip.interapp.combi.bap.audio.data.CombiBAPTMCInfoMessage
 *  de.audi.atip.interapp.combi.bap.data.CombiBAPArrayElement
 *  de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi
 *  de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationInfo
 *  de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationListEntry
 *  de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviDestination
 *  de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData
 *  de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor
 *  de.audi.atip.interapp.combi.bap.navi.data.CombiBAPSemiDynamicRouteInfo
 *  de.audi.atip.interapp.combi.bap.navi.data.EtcStatus
 *  de.audi.atip.log.LogChannel
 *  de.vw.mib.bap.generated.navsd.serializer.ActiveRgType_Status
 *  de.vw.mib.bap.generated.navsd.serializer.Altitude_Status
 *  de.vw.mib.bap.generated.navsd.serializer.CompassInfo_Status
 *  de.vw.mib.bap.generated.navsd.serializer.CurrentPositionInfo_Status
 *  de.vw.mib.bap.generated.navsd.serializer.DestinationInfo_Status
 *  de.vw.mib.bap.generated.navsd.serializer.DistanceToDestination_Status
 *  de.vw.mib.bap.generated.navsd.serializer.DistanceToNextManeuver_Status
 *  de.vw.mib.bap.generated.navsd.serializer.ETC_Status_Status
 *  de.vw.mib.bap.generated.navsd.serializer.Exitview_Status
 *  de.vw.mib.bap.generated.navsd.serializer.FSG_Setup_Status
 *  de.vw.mib.bap.generated.navsd.serializer.InfoStates_Status
 *  de.vw.mib.bap.generated.navsd.serializer.ManeuverDescriptor_Status
 *  de.vw.mib.bap.generated.navsd.serializer.ManeuverState_Status
 *  de.vw.mib.bap.generated.navsd.serializer.MapColorAndType_Status
 *  de.vw.mib.bap.generated.navsd.serializer.MapScale_Status
 *  de.vw.mib.bap.generated.navsd.serializer.MapViewAndOrientation_Status
 *  de.vw.mib.bap.generated.navsd.serializer.Map_Presentation_Status
 *  de.vw.mib.bap.generated.navsd.serializer.OnlineNavigationState_Status
 *  de.vw.mib.bap.generated.navsd.serializer.POI_Search_Result
 *  de.vw.mib.bap.generated.navsd.serializer.RG_Status_Status
 *  de.vw.mib.bap.generated.navsd.serializer.RepeatLastNavAnnouncement_Result
 *  de.vw.mib.bap.generated.navsd.serializer.SemidynamicRouteGuidance_Status
 *  de.vw.mib.bap.generated.navsd.serializer.TimeToDestination_Status
 *  de.vw.mib.bap.generated.navsd.serializer.TrafficBlock_Indication_Status
 *  de.vw.mib.bap.generated.navsd.serializer.TurnToInfo_Status
 *  de.vw.mib.bap.generated.navsd.serializer.VoiceGuidance_Status
 *  de.vw.mib.bap.requests.ResultMethod
 *  de.vw.mib.bap.requests.StatusProperty
 */
package de.audi.app.combi.bap.app.navi;

import de.audi.app.bap.fw.AbstractBAPModuleFSG;
import de.audi.app.bap.fw.AbstractBAPModuleInitializationManagerFSG;
import de.audi.app.bap.fw.functionsync.IFunctionSynchronizationHandler;
import de.audi.app.bap.fw.functiontypes.BAPFunctionArrayFSG;
import de.audi.app.bap.fw.functiontypes.BAPFunctionMethodFSG;
import de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG;
import de.audi.app.combi.bap.AbstractCombiConnector;
import de.audi.app.combi.bap.app.navi.CombiModuleNavi;
import de.audi.app.combi.bap.app.navi.list.AddressListArrayHandler;
import de.audi.app.combi.bap.app.navi.list.FavoriteDestinationsListHandler;
import de.audi.app.combi.bap.app.navi.list.LaneGuidanceHandler;
import de.audi.app.combi.bap.app.navi.list.LastDestinationsListHandler;
import de.audi.app.combi.bap.utils.CombiLogger;
import de.audi.atip.interapp.bap.BAPServiceListener;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPTMCInfoMessage;
import de.audi.atip.interapp.combi.bap.data.CombiBAPArrayElement;
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
import de.vw.mib.bap.requests.ResultMethod;
import de.vw.mib.bap.requests.StatusProperty;
import java.util.Date;
import java.util.GregorianCalendar;

public class AppConnectorNavi
extends AbstractCombiConnector
implements CombiBAPServiceNavi {
    private final LogChannel logChannelFrequent = CombiLogger.getFwNaviFrequentLog();
    private static final int[] COMPASS_SYMBOLIC_2_ANGLE = new int[16];

    protected AppConnectorNavi(CombiModuleNavi combiModuleNavi) {
        super((AbstractBAPModuleFSG)combiModuleNavi);
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

    public void updateCompassInfo(int n, int n2) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateCompassInfo] called (directionAngle=%1, directionSymbolic=%2", (long)n, (long)n2);
        CompassInfo_Status compassInfo_Status = new CompassInfo_Status();
        compassInfo_Status.direction_Angle = n >= 0 && n <= 360 ? n : this.convertDirectionSymbolicToAngle(n2);
        compassInfo_Status.direction_Symbolic = n2 == 255 ? this.convertDirectionAngleToSymbolic(n) : n2;
        this.moduleFsg.getBAPFunctionPropertyFSG(16).sendStatusIfChanged((StatusProperty)compassInfo_Status);
    }

    private int convertDirectionSymbolicToAngle(int n) {
        int n2 = n >= 0 && n < COMPASS_SYMBOLIC_2_ANGLE.length ? COMPASS_SYMBOLIC_2_ANGLE[n] : 65535;
        this.logChannel.log(10000000, "[AppConnectorNavi#convertDirectionSymbolicToAngle] symbolic=%1 -> angle=%2", (long)n, (long)n2);
        return n2;
    }

    private int convertDirectionAngleToSymbolic(int n) {
        int n2;
        int n3 = (int)(((float)n + 11.25f) % 360.0f / 22.5f);
        switch (n3) {
            case 0: {
                n2 = 0;
                break;
            }
            case 1: {
                n2 = 15;
                break;
            }
            case 2: {
                n2 = 14;
                break;
            }
            case 3: {
                n2 = 13;
                break;
            }
            case 4: {
                n2 = 12;
                break;
            }
            case 5: {
                n2 = 11;
                break;
            }
            case 6: {
                n2 = 10;
                break;
            }
            case 7: {
                n2 = 9;
                break;
            }
            case 8: {
                n2 = 8;
                break;
            }
            case 9: {
                n2 = 7;
                break;
            }
            case 10: {
                n2 = 6;
                break;
            }
            case 11: {
                n2 = 5;
                break;
            }
            case 12: {
                n2 = 4;
                break;
            }
            case 13: {
                n2 = 3;
                break;
            }
            case 14: {
                n2 = 2;
                break;
            }
            case 15: {
                n2 = 1;
                break;
            }
            default: {
                n2 = 255;
            }
        }
        this.logChannel.log(10000000, "[AppConnectorNavi#convertDirectionAngleToSymbolic] angle=%1 -> symbolic=%2", (long)n, (long)n2);
        return n2;
    }

    public void updateRGStatus(int n) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateRGStatus] called (rgStatus=%1)", (long)n);
        IFunctionSynchronizationHandler iFunctionSynchronizationHandler = this.moduleFsg.getFunctionSynchronizationHandler();
        iFunctionSynchronizationHandler.startSync(0);
        RG_Status_Status rG_Status_Status = new RG_Status_Status();
        rG_Status_Status.rg_Status = n;
        this.moduleFsg.getBAPFunctionPropertyFSG(17).sendStatusIfChanged((StatusProperty)rG_Status_Status);
    }

    public void updateActiveRGType(int n) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateActiveRGType] called (rgType=%1)", (long)n);
        ActiveRgType_Status activeRgType_Status = new ActiveRgType_Status();
        activeRgType_Status.rgtype = n;
        this.moduleFsg.getBAPFunctionPropertyFSG(39).sendStatusIfChanged((StatusProperty)activeRgType_Status);
    }

    public void updateDistanceToNextManeuver(int n, int n2, boolean bl, int n3) {
        int n4;
        this.logChannelFrequent.log(10000000, "[AppConnectorNavi#updateDistanceToNextManeuver] called (distance=%1, unit=%2, ...", (long)n, (long)n2);
        this.logChannelFrequent.log(10000000, "[AppConnectorNavi#updateDistanceToNextManeuver] ... BGEnabled=%1, BGValue=%2)", bl, (long)n3);
        DistanceToNextManeuver_Status distanceToNextManeuver_Status = new DistanceToNextManeuver_Status();
        int n5 = n4 = bl ? 1 : 0;
        if (n == -1) {
            distanceToNextManeuver_Status.distanceToNextManeuver.distance = 0;
            distanceToNextManeuver_Status.distanceToNextManeuver.unit = 255;
            distanceToNextManeuver_Status.validityInformation.distanceToNextManeuverValid = false;
        } else {
            distanceToNextManeuver_Status.distanceToNextManeuver.distance = n;
            distanceToNextManeuver_Status.distanceToNextManeuver.unit = n2;
            distanceToNextManeuver_Status.validityInformation.distanceToNextManeuverValid = true;
        }
        distanceToNextManeuver_Status.bargraphInfo.bargraphOnOff = n4;
        distanceToNextManeuver_Status.bargraphInfo.bargraph = n3;
        this.moduleFsg.getBAPFunctionPropertyFSG(18).sendStatusIfChanged((StatusProperty)distanceToNextManeuver_Status);
    }

    public void updateCurrentPositionInfo(String string) {
        this.logChannel.log(10000000, "[AppConnectorNavi#updateCurrentPositionInfo] called (currentPositionInfo=%1)", (Object)string);
        CurrentPositionInfo_Status currentPositionInfo_Status = new CurrentPositionInfo_Status();
        currentPositionInfo_Status.positionInfo.setContent(string);
        this.moduleFsg.getBAPFunctionPropertyFSG(19).sendStatusIfChanged((StatusProperty)currentPositionInfo_Status);
    }

    public void updateTurnToInfo(String string, String string2) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateTurnToInfo] called (turnToInfo=%1, signPost=%2)", (Object)string, (Object)string2);
        TurnToInfo_Status turnToInfo_Status = new TurnToInfo_Status();
        turnToInfo_Status.turnToInfo.setContent(string);
        turnToInfo_Status.signPost.setContent(string2);
        this.moduleFsg.getBAPFunctionPropertyFSG(20).sendStatusIfChanged((StatusProperty)turnToInfo_Status);
    }

    public void updateDistanceToDestination(int n, int n2, boolean bl) {
        this.logChannelFrequent.log(10000000, "[AppConnectorNavi#updateDistanceToDestination] called (distance=%1, unit=%2, isDistanceToStopOver=%3)", (long)n, (long)n2, bl);
        DistanceToDestination_Status distanceToDestination_Status = new DistanceToDestination_Status();
        if (n == -1) {
            distanceToDestination_Status.distanceToDestination.distance = 0;
            distanceToDestination_Status.distanceToDestination.unit = 255;
            distanceToDestination_Status.distanceToDestinationType.distanceToStopover = false;
            distanceToDestination_Status.validityInformation.distanceToDestinationValid = false;
        } else {
            distanceToDestination_Status.distanceToDestination.distance = n;
            distanceToDestination_Status.distanceToDestination.unit = n2;
            distanceToDestination_Status.distanceToDestinationType.distanceToStopover = bl;
            distanceToDestination_Status.validityInformation.distanceToDestinationValid = true;
        }
        this.moduleFsg.getBAPFunctionPropertyFSG(21).sendStatusIfChanged((StatusProperty)distanceToDestination_Status);
    }

    public void updateTimeToDestination(int n, int n2, long l) {
        boolean bl;
        this.logChannelFrequent.log(10000000, "[AppConnectorNavi#updateTimeToDestination] called (timeInfoType=%1, navigationTimeFormat=%2, ...", (long)n, (long)n2);
        this.logChannelFrequent.log(10000000, "[AppConnectorNavi#updateTimeToDestination] ... time=%1)", l);
        TimeToDestination_Status timeToDestination_Status = new TimeToDestination_Status();
        timeToDestination_Status.timeInfo.timeInfoType = n;
        timeToDestination_Status.timeInfo.navigationTimeFormat = n2;
        boolean bl2 = bl = l >= 0L;
        if (bl) {
            boolean bl3;
            boolean bl4 = bl3 = n == 1;
            if (bl3) {
                Date date = new Date(l * 1000L);
                GregorianCalendar gregorianCalendar = new GregorianCalendar();
                gregorianCalendar.setTime(date);
                timeToDestination_Status.timeInfo.year = gregorianCalendar.get(1) % 100;
                timeToDestination_Status.timeInfo.month = gregorianCalendar.get(2) + 1;
                timeToDestination_Status.timeInfo.day = gregorianCalendar.get(5);
                timeToDestination_Status.timeInfo.hour = gregorianCalendar.get(11);
                timeToDestination_Status.timeInfo.minute = gregorianCalendar.get(12);
            } else {
                int n3 = (int)(l / 60L);
                timeToDestination_Status.timeInfo.hour = n3 / 60;
                timeToDestination_Status.timeInfo.minute = n3 % 60;
            }
            timeToDestination_Status.validityInformation.timeInfo_YearIsAvailableToBeDisplayed = bl3;
            timeToDestination_Status.validityInformation.timeInfo_MonthIsAvailableToBeDisplayed = bl3;
            timeToDestination_Status.validityInformation.timeInfo_DayIsAvailableToBeDisplayed = bl3;
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
        this.moduleFsg.getBAPFunctionPropertyFSG(22).sendStatusIfChanged((StatusProperty)timeToDestination_Status);
    }

    public void updateManeuverDescriptor(CombiBAPNaviManeuverDescriptor[] combiBAPNaviManeuverDescriptorArray) {
        IFunctionSynchronizationHandler iFunctionSynchronizationHandler;
        this.logChannel.log(10000000, "[AppConnectorNavi#updateManeuverDescriptor] called");
        if (combiBAPNaviManeuverDescriptorArray == null) {
            this.logChannel.log(10000, "[AppConnectorNavi#updateManeuverDescriptor] maneuver descriptor is null");
            return;
        }
        ManeuverDescriptor_Status maneuverDescriptor_Status = this.createManeuverDescriptorStatus(combiBAPNaviManeuverDescriptorArray);
        if (AppConnectorNavi.maneuverDescriptorNeedsToBeSynchronized(maneuverDescriptor_Status) && (!(iFunctionSynchronizationHandler = this.moduleFsg.getFunctionSynchronizationHandler()).isSyncActive() || iFunctionSynchronizationHandler.getCurrentSyncType() != 0)) {
            iFunctionSynchronizationHandler.startSync(1);
        }
        this.moduleFsg.getBAPFunctionPropertyFSG(23).sendStatusIfChanged((StatusProperty)maneuverDescriptor_Status);
    }

    private static boolean maneuverDescriptorNeedsToBeSynchronized(ManeuverDescriptor_Status maneuverDescriptor_Status) {
        return maneuverDescriptor_Status.maneuver_1.mainElement != 6 && maneuverDescriptor_Status.maneuver_1.mainElement != 9 && maneuverDescriptor_Status.maneuver_1.mainElement != 10;
    }

    public void updateManeuverDescriptorAndExitView(CombiBAPNaviManeuverDescriptor[] combiBAPNaviManeuverDescriptorArray, int n, int n2) {
        this.logChannel.log(100000, "[AppConnectorNavi#updateManeuverDescriptorAndExitView] deprecated. variant=%1, exitviewID=%2", (long)n, (long)n2);
    }

    private ManeuverDescriptor_Status createManeuverDescriptorStatus(CombiBAPNaviManeuverDescriptor[] combiBAPNaviManeuverDescriptorArray) {
        ManeuverDescriptor_Status maneuverDescriptor_Status = new ManeuverDescriptor_Status();
        if (combiBAPNaviManeuverDescriptorArray.length > 0 && combiBAPNaviManeuverDescriptorArray[0] != null) {
            this.logChannel.log(10000000, "[AppConnectorNavi#createManeuverDescriptorStatus] maneuver 1 = %1", (Object)combiBAPNaviManeuverDescriptorArray[0]);
            maneuverDescriptor_Status.maneuver_1.mainElement = AppConnectorNavi.getMappedManeuverMainElement(combiBAPNaviManeuverDescriptorArray[0].mainElement, combiBAPNaviManeuverDescriptorArray[0].direction);
            maneuverDescriptor_Status.maneuver_1.direction = combiBAPNaviManeuverDescriptorArray[0].direction;
            maneuverDescriptor_Status.maneuver_1.zLevelGuidance = combiBAPNaviManeuverDescriptorArray[0].zLevelGuidance;
            maneuverDescriptor_Status.maneuver_1.sidestreets.setContent(combiBAPNaviManeuverDescriptorArray[0].sideStreets);
        } else {
            this.logChannel.log(10000000, "[AppConnectorNavi#createManeuverDescriptorStatus] reset maneuver 1");
            maneuverDescriptor_Status.maneuver_1.mainElement = 0;
            maneuverDescriptor_Status.maneuver_1.direction = 0;
            maneuverDescriptor_Status.maneuver_1.zLevelGuidance = 0;
            maneuverDescriptor_Status.maneuver_1.sidestreets.setContent("isNativeLittleEndian");
        }
        if (combiBAPNaviManeuverDescriptorArray.length > 1 && combiBAPNaviManeuverDescriptorArray[1] != null) {
            this.logChannel.log(10000000, "[AppConnectorNavi#createManeuverDescriptorStatus] maneuver 2 = %1", (Object)combiBAPNaviManeuverDescriptorArray[1]);
            maneuverDescriptor_Status.maneuver_2.mainElement = AppConnectorNavi.getMappedManeuverMainElement(combiBAPNaviManeuverDescriptorArray[1].mainElement, combiBAPNaviManeuverDescriptorArray[1].direction);
            maneuverDescriptor_Status.maneuver_2.direction = combiBAPNaviManeuverDescriptorArray[1].direction;
            maneuverDescriptor_Status.maneuver_2.zLevelGuidance = combiBAPNaviManeuverDescriptorArray[1].zLevelGuidance;
            maneuverDescriptor_Status.maneuver_2.sidestreets.setContent(combiBAPNaviManeuverDescriptorArray[1].sideStreets);
        } else {
            this.logChannel.log(10000000, "[AppConnectorNavi#createManeuverDescriptorStatus] reset maneuver 2");
            maneuverDescriptor_Status.maneuver_2.mainElement = 0;
            maneuverDescriptor_Status.maneuver_2.direction = 0;
            maneuverDescriptor_Status.maneuver_2.zLevelGuidance = 0;
            maneuverDescriptor_Status.maneuver_2.sidestreets.setContent("isNativeLittleEndian");
        }
        if (combiBAPNaviManeuverDescriptorArray.length > 2 && combiBAPNaviManeuverDescriptorArray[2] != null) {
            this.logChannel.log(10000000, "[AppConnectorNavi#createManeuverDescriptorStatus] maneuver 3 = %1", (Object)combiBAPNaviManeuverDescriptorArray[2]);
            maneuverDescriptor_Status.maneuver_3.mainElement = AppConnectorNavi.getMappedManeuverMainElement(combiBAPNaviManeuverDescriptorArray[2].mainElement, combiBAPNaviManeuverDescriptorArray[2].direction);
            maneuverDescriptor_Status.maneuver_3.direction = combiBAPNaviManeuverDescriptorArray[2].direction;
            maneuverDescriptor_Status.maneuver_3.zLevelGuidance = combiBAPNaviManeuverDescriptorArray[2].zLevelGuidance;
            maneuverDescriptor_Status.maneuver_3.sidestreets.setContent(combiBAPNaviManeuverDescriptorArray[2].sideStreets);
        } else {
            this.logChannel.log(10000000, "[AppConnectorNavi#createManeuverDescriptorStatus] reset maneuver 3");
            maneuverDescriptor_Status.maneuver_3.mainElement = 0;
            maneuverDescriptor_Status.maneuver_3.direction = 0;
            maneuverDescriptor_Status.maneuver_3.zLevelGuidance = 0;
            maneuverDescriptor_Status.maneuver_3.sidestreets.setContent("isNativeLittleEndian");
        }
        return maneuverDescriptor_Status;
    }

    private static int getMappedManeuverMainElement(int n, int n2) {
        int n3 = n;
        if (n == 28 && (n2 == 64 || n2 == 192)) {
            n3 = 12;
        }
        return n3;
    }

    public void updateLaneGuidance(boolean bl, CombiBAPNaviLaneGuidanceData[] combiBAPNaviLaneGuidanceDataArray) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateLaneGuidance] called (enableLaneGuidance=%1, dataSize=%2)", bl, combiBAPNaviLaneGuidanceDataArray == null ? 0L : (long)combiBAPNaviLaneGuidanceDataArray.length);
        BAPFunctionArrayFSG bAPFunctionArrayFSG = this.moduleFsg.getBAPFunctionArrayFSG(24);
        LaneGuidanceHandler laneGuidanceHandler = (LaneGuidanceHandler)bAPFunctionArrayFSG.getArrayHandler();
        if (laneGuidanceHandler != null) {
            laneGuidanceHandler.setLaneGuidanceEnabled(bl);
            laneGuidanceHandler.updateList((CombiBAPArrayElement[])combiBAPNaviLaneGuidanceDataArray);
        }
    }

    public void updateTMCInfoMessages(CombiBAPTMCInfoMessage[] combiBAPTMCInfoMessageArray) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateTMCInfoMessages] called (noOfMessages=%1)", combiBAPTMCInfoMessageArray == null ? 0L : (long)combiBAPTMCInfoMessageArray.length);
        ((CombiModuleNavi)this.moduleFsg).getTMCInfoHandler().updateTMCInfoMessages(combiBAPTMCInfoMessageArray);
    }

    public void routeGuidanceActDeactResult(int n) {
        this.logChannel.log(1000000, "[AppConnectorNavi#routeGuidanceActDeactResult] called (result=%1)", (long)n);
        ((CombiModuleNavi)this.moduleFsg).rgActDeactResultReceived(n);
    }

    public void repeatLastNavAnnouncementResult(int n) {
        this.logChannel.log(1000000, "[AppConnectorNavi#repeatLastNavAnnouncementResult] called (result=%1)", (long)n);
        BAPFunctionMethodFSG bAPFunctionMethodFSG = this.moduleFsg.getBAPFunctionMethodFSG(35);
        RepeatLastNavAnnouncement_Result repeatLastNavAnnouncement_Result = (RepeatLastNavAnnouncement_Result)this.moduleFsg.createResultSerializer(35);
        repeatLastNavAnnouncement_Result.repeatLna_Result = n;
        bAPFunctionMethodFSG.resultREQ((ResultMethod)repeatLastNavAnnouncement_Result);
    }

    public void updateVoiceGuidanceState(int n) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateVoiceGuidanceState] called (state=%1)", (long)n);
        VoiceGuidance_Status voiceGuidance_Status = new VoiceGuidance_Status();
        voiceGuidance_Status.voiceGuidance_State = n;
        this.moduleFsg.getBAPFunctionPropertyFSG(36).sendStatusIfChanged((StatusProperty)voiceGuidance_Status);
    }

    public void updateInfoStates(int n) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateInfoStates] called (states=%1)", (long)n);
        InfoStates_Status infoStates_Status = new InfoStates_Status();
        infoStates_Status.states = n;
        this.moduleFsg.getBAPFunctionPropertyFSG(38).sendStatusIfChanged((StatusProperty)infoStates_Status);
    }

    public void updateTrafficBlockIndication(int n) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateTrafficBlockIndication] called (tmcSymbol=%1)", (long)n);
        TrafficBlock_Indication_Status trafficBlock_Indication_Status = new TrafficBlock_Indication_Status();
        trafficBlock_Indication_Status.tmc_Symbol = n;
        this.moduleFsg.getBAPFunctionPropertyFSG(40).sendStatusIfChanged((StatusProperty)trafficBlock_Indication_Status);
    }

    public void responseLastDestinationsList(int n, CombiBAPDestinationListEntry[] combiBAPDestinationListEntryArray) {
    }

    public void responseFavoriteDestinationsList(int n, CombiBAPDestinationListEntry[] combiBAPDestinationListEntryArray) {
    }

    public void updateLastDestinationsList(CombiBAPDestinationListEntry[] combiBAPDestinationListEntryArray) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateLastDestinationsList] called (dataSize=%1)", combiBAPDestinationListEntryArray == null ? 0L : (long)combiBAPDestinationListEntryArray.length);
        BAPFunctionArrayFSG bAPFunctionArrayFSG = this.moduleFsg.getBAPFunctionArrayFSG(29);
        LastDestinationsListHandler lastDestinationsListHandler = (LastDestinationsListHandler)bAPFunctionArrayFSG.getArrayHandler();
        if (lastDestinationsListHandler != null) {
            lastDestinationsListHandler.updateList((CombiBAPArrayElement[])combiBAPDestinationListEntryArray);
        } else {
            this.logChannel.log(10000, "[AppConnectorNavi#updateFavoriteDestinationsList] no array handler registered for BAP_FCT_ID_LAST_DEST_LIST");
        }
    }

    public void updateFavoriteDestinationsList(CombiBAPDestinationListEntry[] combiBAPDestinationListEntryArray) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateFavoriteDestinationsList] called (dataSize=%1)", combiBAPDestinationListEntryArray == null ? 0L : (long)combiBAPDestinationListEntryArray.length);
        BAPFunctionArrayFSG bAPFunctionArrayFSG = this.moduleFsg.getBAPFunctionArrayFSG(30);
        FavoriteDestinationsListHandler favoriteDestinationsListHandler = (FavoriteDestinationsListHandler)bAPFunctionArrayFSG.getArrayHandler();
        if (favoriteDestinationsListHandler != null) {
            favoriteDestinationsListHandler.updateList((CombiBAPArrayElement[])combiBAPDestinationListEntryArray);
        } else {
            this.logChannel.log(10000, "[AppConnectorNavi#updateFavoriteDestinationsList] no array handler registered for BAP_FCT_ID_FAVORITE_DEST_LIST");
        }
    }

    public void updateHomeAddress(CombiBAPNaviDestination combiBAPNaviDestination) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateHomeAddress] homeAddress=%1 ", (Object)(combiBAPNaviDestination != null ? combiBAPNaviDestination.toString() : "null!"));
        AddressListArrayHandler addressListArrayHandler = (AddressListArrayHandler)this.moduleFsg.getBAPFunctionArrayFSG(33).getArrayHandler();
        addressListArrayHandler.updateHomeAddress(combiBAPNaviDestination);
    }

    public void updateMapColor(int n) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapColor] color=%1", (long)n);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(43);
        MapColorAndType_Status mapColorAndType_Status = (MapColorAndType_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapColorAndType_Status mapColorAndType_Status2 = new MapColorAndType_Status();
        mapColorAndType_Status2.activeMapType = mapColorAndType_Status.activeMapType;
        mapColorAndType_Status2.mainMapSetup = mapColorAndType_Status.mainMapSetup;
        mapColorAndType_Status2.supportedMapTypes.rangeMapIsSupported = mapColorAndType_Status.supportedMapTypes.rangeMapIsSupported;
        mapColorAndType_Status2.supportedMapTypes.overviewMapIsSupported = mapColorAndType_Status.supportedMapTypes.overviewMapIsSupported;
        mapColorAndType_Status2.supportedMapTypes.position3DMapIsSupported = mapColorAndType_Status.supportedMapTypes.position3DMapIsSupported;
        mapColorAndType_Status2.supportedMapTypes.position2DMapIsSupported = mapColorAndType_Status.supportedMapTypes.position2DMapIsSupported;
        mapColorAndType_Status2.supportedMapTypes.destinationMapIsSupported = mapColorAndType_Status.supportedMapTypes.destinationMapIsSupported;
        mapColorAndType_Status2.supportedMapTypes.mainMapSetupIsSupported = mapColorAndType_Status.supportedMapTypes.mainMapSetupIsSupported;
        mapColorAndType_Status2.colour = n;
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)mapColorAndType_Status2);
    }

    public void updateMapType(int n, int n2) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapType] activeMapType=%1, mainMapSetup=%2", (long)n, (long)n2);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(43);
        MapColorAndType_Status mapColorAndType_Status = (MapColorAndType_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapColorAndType_Status mapColorAndType_Status2 = new MapColorAndType_Status();
        if (this.logChannel.isInfo() && mapColorAndType_Status.mainMapSetup != mapColorAndType_Status2.mainMapSetup) {
            String string;
            switch (mapColorAndType_Status2.mainMapSetup) {
                case 0: {
                    string = "NO MAP IN ASG";
                    break;
                }
                case 1: {
                    string = "MAIN MAP IN ASG";
                    break;
                }
                case 2: {
                    string = "MAIN MAP IN FSG";
                    break;
                }
                default: {
                    string = "NOT SUPPORTED OR INVALID VALUE";
                }
            }
            this.logChannel.log(1000000, "[AppConnectorNavi#updateMapType] main map setup changed: %1", (Object)string);
        }
        mapColorAndType_Status2.colour = mapColorAndType_Status.colour;
        mapColorAndType_Status2.supportedMapTypes.rangeMapIsSupported = mapColorAndType_Status.supportedMapTypes.rangeMapIsSupported;
        mapColorAndType_Status2.supportedMapTypes.overviewMapIsSupported = mapColorAndType_Status.supportedMapTypes.overviewMapIsSupported;
        mapColorAndType_Status2.supportedMapTypes.position3DMapIsSupported = mapColorAndType_Status.supportedMapTypes.position3DMapIsSupported;
        mapColorAndType_Status2.supportedMapTypes.position2DMapIsSupported = mapColorAndType_Status.supportedMapTypes.position2DMapIsSupported;
        mapColorAndType_Status2.supportedMapTypes.destinationMapIsSupported = mapColorAndType_Status.supportedMapTypes.destinationMapIsSupported;
        mapColorAndType_Status2.supportedMapTypes.mainMapSetupIsSupported = mapColorAndType_Status.supportedMapTypes.mainMapSetupIsSupported;
        mapColorAndType_Status2.activeMapType = n;
        mapColorAndType_Status2.mainMapSetup = n2;
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)mapColorAndType_Status2);
    }

    public void updateSupportedMapTypes(boolean bl, int n) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateSupportedMapTypes] mainMapSupported=%1, supportedMapTypes=%2", bl, (long)n);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(43);
        MapColorAndType_Status mapColorAndType_Status = (MapColorAndType_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapColorAndType_Status mapColorAndType_Status2 = new MapColorAndType_Status();
        mapColorAndType_Status2.colour = mapColorAndType_Status.colour;
        mapColorAndType_Status2.activeMapType = mapColorAndType_Status.activeMapType;
        mapColorAndType_Status2.mainMapSetup = mapColorAndType_Status.mainMapSetup;
        mapColorAndType_Status2.supportedMapTypes.rangeMapIsSupported = false;
        mapColorAndType_Status2.supportedMapTypes.overviewMapIsSupported = this.hasAttribute(n, 8);
        mapColorAndType_Status2.supportedMapTypes.position3DMapIsSupported = this.hasAttribute(n, 4);
        mapColorAndType_Status2.supportedMapTypes.position2DMapIsSupported = this.hasAttribute(n, 2);
        mapColorAndType_Status2.supportedMapTypes.destinationMapIsSupported = this.hasAttribute(n, 1);
        mapColorAndType_Status2.supportedMapTypes.mainMapSetupIsSupported = bl;
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)mapColorAndType_Status2);
    }

    public void updateMapView(int n, int n2) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapView] mapView=%1, supplementaryMapView=%2", (long)n, (long)n2);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(44);
        MapViewAndOrientation_Status mapViewAndOrientation_Status = (MapViewAndOrientation_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapViewAndOrientation_Status mapViewAndOrientation_Status2 = AppConnectorNavi.cloneMapViewAndOrientationStatus(mapViewAndOrientation_Status);
        mapViewAndOrientation_Status2.mapView = n;
        mapViewAndOrientation_Status2.supplementaryMapView = n2;
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)mapViewAndOrientation_Status2);
    }

    public void updateSupportedMapViews(int n, int n2) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateSupportedMapViews] supportedMapViews=%1, supportedSupplementaryMapViews=%2", (long)n, (long)n2);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(44);
        MapViewAndOrientation_Status mapViewAndOrientation_Status = (MapViewAndOrientation_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapViewAndOrientation_Status mapViewAndOrientation_Status2 = AppConnectorNavi.cloneMapViewAndOrientationStatus(mapViewAndOrientation_Status);
        mapViewAndOrientation_Status2.supportedMapViews.trafficMapIsSupported = this.hasAttribute(n, 4);
        mapViewAndOrientation_Status2.supportedMapViews.googleEarthMapIsSupported = this.hasAttribute(n, 2);
        mapViewAndOrientation_Status2.supportedMapViews.standardMapIsSupported = this.hasAttribute(n, 1);
        mapViewAndOrientation_Status2.supportedSupplementaryMapViews._1BoxSupported = this.hasAttribute(n2, 4);
        mapViewAndOrientation_Status2.supportedSupplementaryMapViews.compassSupported = this.hasAttribute(n2, 2);
        mapViewAndOrientation_Status2.supportedSupplementaryMapViews.intersectionZoomSupported = this.hasAttribute(n2, 1);
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)mapViewAndOrientation_Status2);
    }

    public void updateMapVisibility(boolean bl, boolean bl2) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapVisibility] lvdsMapVisible=%1, supplementaryMapViewVisible=%2", bl, bl2);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(44);
        MapViewAndOrientation_Status mapViewAndOrientation_Status = (MapViewAndOrientation_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapViewAndOrientation_Status mapViewAndOrientation_Status2 = AppConnectorNavi.cloneMapViewAndOrientationStatus(mapViewAndOrientation_Status);
        mapViewAndOrientation_Status2.mapVisibility.lvdsMapIsVisible = bl;
        mapViewAndOrientation_Status2.mapVisibility.supplementaryMapViewIsVisible = bl2;
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)mapViewAndOrientation_Status2);
    }

    public void updateMapOrientation(int n) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapOrientation] orientation=%1", (long)n);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(44);
        MapViewAndOrientation_Status mapViewAndOrientation_Status = (MapViewAndOrientation_Status)bAPFunctionPropertyFSG.getLastStatus();
        MapViewAndOrientation_Status mapViewAndOrientation_Status2 = AppConnectorNavi.cloneMapViewAndOrientationStatus(mapViewAndOrientation_Status);
        mapViewAndOrientation_Status2.mapOrientation = n;
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)mapViewAndOrientation_Status2);
    }

    private static MapViewAndOrientation_Status cloneMapViewAndOrientationStatus(MapViewAndOrientation_Status mapViewAndOrientation_Status) {
        MapViewAndOrientation_Status mapViewAndOrientation_Status2 = new MapViewAndOrientation_Status();
        mapViewAndOrientation_Status2.supportedMapViews.trafficMapIsSupported = mapViewAndOrientation_Status.supportedMapViews.trafficMapIsSupported;
        mapViewAndOrientation_Status2.supportedMapViews.googleEarthMapIsSupported = mapViewAndOrientation_Status.supportedMapViews.googleEarthMapIsSupported;
        mapViewAndOrientation_Status2.supportedMapViews.standardMapIsSupported = mapViewAndOrientation_Status.supportedMapViews.standardMapIsSupported;
        mapViewAndOrientation_Status2.supportedSupplementaryMapViews._1BoxSupported = mapViewAndOrientation_Status.supportedSupplementaryMapViews._1BoxSupported;
        mapViewAndOrientation_Status2.supportedSupplementaryMapViews.compassSupported = mapViewAndOrientation_Status.supportedSupplementaryMapViews.compassSupported;
        mapViewAndOrientation_Status2.supportedSupplementaryMapViews.intersectionZoomSupported = mapViewAndOrientation_Status.supportedSupplementaryMapViews.intersectionZoomSupported;
        mapViewAndOrientation_Status2.mapVisibility.supplementaryMapViewIsVisible = mapViewAndOrientation_Status.mapVisibility.supplementaryMapViewIsVisible;
        mapViewAndOrientation_Status2.mapVisibility.lvdsMapIsVisible = mapViewAndOrientation_Status.mapVisibility.lvdsMapIsVisible;
        mapViewAndOrientation_Status2.modification.googleMapCanNotBeModified = mapViewAndOrientation_Status.modification.googleMapCanNotBeModified;
        mapViewAndOrientation_Status2.mapView = mapViewAndOrientation_Status.mapView;
        mapViewAndOrientation_Status2.supplementaryMapView = mapViewAndOrientation_Status.supplementaryMapView;
        mapViewAndOrientation_Status2.mapOrientation = mapViewAndOrientation_Status.mapOrientation;
        return mapViewAndOrientation_Status2;
    }

    public void updateMapScale(int n, boolean bl, int n2, int n3, boolean bl2) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapScale] autoZoomSetting=%1, autoZoomEnabled=%3, zoomLevel=%2", (long)n, (long)n2, bl);
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapScale] unit=%2, intersectionAutoZoomSupported=%1", bl2, (long)n3);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(45);
        MapScale_Status mapScale_Status = new MapScale_Status();
        mapScale_Status.autoZoom = n;
        mapScale_Status.autoZoomState.autoZoomActive = bl;
        mapScale_Status.scale = n2;
        mapScale_Status.unit = n3;
        mapScale_Status.supportedAutoZoom.autozoomOnForIntersectionSupported = bl2;
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)mapScale_Status);
    }

    public void updateDestinationInfo(CombiBAPDestinationInfo combiBAPDestinationInfo) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateDestinationInfo] destInfo=%1", (Object)combiBAPDestinationInfo);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(46);
        DestinationInfo_Status destinationInfo_Status = new DestinationInfo_Status();
        destinationInfo_Status.position.latitude = (int)(combiBAPDestinationInfo.getDestination().getLatitude() * 1000000.0f);
        destinationInfo_Status.position.longitude = (int)(combiBAPDestinationInfo.getDestination().getLongitude() * 1000000.0f);
        destinationInfo_Status.totalNumOfStopovers = combiBAPDestinationInfo.getNoOfStopovers();
        destinationInfo_Status.stopover_Sn = combiBAPDestinationInfo.getNoOfNextStopover();
        destinationInfo_Status.poi_Type = combiBAPDestinationInfo.getDestination().getPOIType();
        destinationInfo_Status.poi_Description.setContent(combiBAPDestinationInfo.getDestination().getPOIDescription());
        destinationInfo_Status.street.setContent(combiBAPDestinationInfo.getDestination().getStreet());
        destinationInfo_Status.town.setContent(combiBAPDestinationInfo.getDestination().getCity());
        destinationInfo_Status.state.setContent(combiBAPDestinationInfo.getDestination().getState());
        destinationInfo_Status.postalCode.setContent(combiBAPDestinationInfo.getDestination().getPostalCode());
        destinationInfo_Status.country.setContent(combiBAPDestinationInfo.getDestination().getCountry());
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)destinationInfo_Status);
    }

    public void updateAltitude(int n, int n2) {
        this.logChannelFrequent.log(1000000, "[AppConnectorNavi#updateAltitude] altitude=%1, unit=%2", (long)n, (long)n2);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(47);
        Altitude_Status altitude_Status = new Altitude_Status();
        altitude_Status.altitude = n;
        altitude_Status.unit = n2;
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)altitude_Status);
    }

    public void updateOnlineNavigationState(int n, int n2, int n3) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateOnlineNavigationState] state=%1, bufferProgress=%2, onlineNavigationSystem=%3", (long)n, (long)n2, (long)n3);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(48);
        OnlineNavigationState_Status onlineNavigationState_Status = new OnlineNavigationState_Status();
        onlineNavigationState_Status.state = n;
        onlineNavigationState_Status.progress = n2;
        onlineNavigationState_Status.onlineNavigationSystem = n3;
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)onlineNavigationState_Status);
    }

    public void updateExitView(int n, int n2) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateExitView] variant=%1, exitviewID=%2", (long)n, (long)n2);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(49);
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)AppConnectorNavi.createExitViewStatus(n, n2));
    }

    private static Exitview_Status createExitViewStatus(int n, int n2) {
        Exitview_Status exitview_Status = new Exitview_Status();
        exitview_Status.variant = n;
        exitview_Status.exitview_Id = n2;
        return exitview_Status;
    }

    public void updateSemidynamicRouteGuidance(CombiBAPSemiDynamicRouteInfo combiBAPSemiDynamicRouteInfo) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateSemidynamicRouteGuidance] routeInfo=%1", (Object)combiBAPSemiDynamicRouteInfo);
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
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)semidynamicRouteGuidance_Status);
    }

    public void poiSearchResult(int n, int n2) {
        this.logChannel.log(1000000, "[AppConnectorNavi#poiSearchResult] result=%1, amountOfFoundEntries=%2", (long)n, (long)n2);
        BAPFunctionMethodFSG bAPFunctionMethodFSG = this.moduleFsg.getBAPFunctionMethodFSG(51);
        POI_Search_Result pOI_Search_Result = (POI_Search_Result)this.moduleFsg.createResultSerializer(51);
        pOI_Search_Result.poi_Search_Result = n;
        pOI_Search_Result.amountOfFoundEntries = n2;
        bAPFunctionMethodFSG.resultREQ((ResultMethod)pOI_Search_Result);
    }

    public void updatePOIListSize(int n) {
        this.logChannel.log(100000, "[AppConnectorNavi#updatePOIListSize] listSize=%1 - NOT IMPLEMENTED YET", (long)n);
    }

    public void updateFSGSetup(int n, boolean bl) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateFSGSetup] voiceGuidanceSetup=%1, poiSearchSupported", (long)n);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(53);
        FSG_Setup_Status fSG_Setup_Status = new FSG_Setup_Status();
        fSG_Setup_Status.supported_Poi_Types.fuelStationAndParkingAreaSupported = bl;
        fSG_Setup_Status.voiceGuidance.voiceGuidanceOnAvailable = this.hasAttribute(n, 1);
        fSG_Setup_Status.voiceGuidance.voiceGuidanceOnReducedAvailable = this.hasAttribute(n, 2);
        fSG_Setup_Status.voiceGuidance.voiceGuidanceOnTrafficAvailable = this.hasAttribute(n, 4);
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)fSG_Setup_Status);
    }

    public void updateMapPresentation(boolean bl, boolean bl2, boolean bl3) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateMapPresentation] largeMapView=%1, leftSideMenuOpen=%2, rightSideMenuOpen=%3", bl, bl2, bl3);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(54);
        Map_Presentation_Status map_Presentation_Status = new Map_Presentation_Status();
        map_Presentation_Status.asg_Hmi_State.largeMapView = bl;
        map_Presentation_Status.asg_Hmi_State.leftSideMenueOpen = bl2;
        map_Presentation_Status.asg_Hmi_State.rightSideMenueOpen = bl3;
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)map_Presentation_Status);
    }

    public void updateManeuverState(int n) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateManeuverState] state: %1", (long)n);
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(55);
        ManeuverState_Status maneuverState_Status = new ManeuverState_Status();
        maneuverState_Status.state = n;
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)maneuverState_Status);
    }

    public void updateEtcStatus(EtcStatus etcStatus) {
        this.logChannel.log(1000000, "[AppConnectorNavi#updateEtcStatus] %1", (Object)etcStatus);
        ETC_Status_Status eTC_Status_Status = new ETC_Status_Status();
        eTC_Status_Status.cardStatus = etcStatus.getCardStatus();
        BAPFunctionPropertyFSG bAPFunctionPropertyFSG = this.moduleFsg.getBAPFunctionPropertyFSG(56);
        bAPFunctionPropertyFSG.sendStatusIfChanged((StatusProperty)eTC_Status_Status);
    }

    static {
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[0] = 0;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[1] = 337;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[2] = 315;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[3] = 292;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[4] = 270;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[5] = 247;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[6] = 225;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[7] = 202;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[8] = 180;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[9] = 157;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[10] = 135;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[11] = 112;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[12] = 90;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[13] = 67;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[14] = 45;
        AppConnectorNavi.COMPASS_SYMBOLIC_2_ANGLE[15] = 22;
    }
}
