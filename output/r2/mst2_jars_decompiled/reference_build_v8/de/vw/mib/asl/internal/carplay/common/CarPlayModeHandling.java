/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  de.vw.mib.asl.api.navigation.ASLNavigationServices
 *  de.vw.mib.asl.framework.internal.framework.ServiceManager
 *  de.vw.mib.asl.internal.carplay.audio.CarPlayAudiohandling
 *  de.vw.mib.asl.internal.carplay.common.CarPlayGlobalProperies
 *  de.vw.mib.asl.internal.carplay.common.CarPlayHMIRequestParameterConfiguration
 *  de.vw.mib.asl.internal.carplay.common.CarPlayHelper
 *  de.vw.mib.asl.internal.carplay.common.CarPlayModeParameter
 *  de.vw.mib.asl.internal.carplay.common.ExboxGuidanceListenerImpl
 *  de.vw.mib.asl.internal.carplay.speech.SpeechApplicant
 *  de.vw.mib.asl.internal.carplay.target.HsmTarget
 *  de.vw.mib.dialogactivation.ActivationState
 *  de.vw.mib.genericevents.EventGeneric
 *  de.vw.mib.log4mib.LogMessage
 *  org.dsi.ifc.carplay.AppState
 *  org.dsi.ifc.carplay.AppStateRequest
 *  org.dsi.ifc.carplay.Resource
 *  org.dsi.ifc.carplay.ResourceRequest
 */
package de.vw.mib.asl.internal.carplay.common;

import de.vw.mib.asl.api.navigation.ASLNavigationServices;
import de.vw.mib.asl.framework.internal.framework.ServiceManager;
import de.vw.mib.asl.internal.carplay.audio.CarPlayAudiohandling;
import de.vw.mib.asl.internal.carplay.common.CarPlayGlobalProperies;
import de.vw.mib.asl.internal.carplay.common.CarPlayHMIRequestParameterConfiguration;
import de.vw.mib.asl.internal.carplay.common.CarPlayHelper;
import de.vw.mib.asl.internal.carplay.common.CarPlayModeParameter;
import de.vw.mib.asl.internal.carplay.common.ExboxGuidanceListenerImpl;
import de.vw.mib.asl.internal.carplay.speech.SpeechApplicant;
import de.vw.mib.asl.internal.carplay.target.HsmTarget;
import de.vw.mib.dialogactivation.ActivationState;
import de.vw.mib.genericevents.EventGeneric;
import de.vw.mib.log4mib.LogMessage;
import org.dsi.ifc.carplay.AppState;
import org.dsi.ifc.carplay.AppStateRequest;
import org.dsi.ifc.carplay.Resource;
import org.dsi.ifc.carplay.ResourceRequest;

public class CarPlayModeHandling {
    public String classname = "CarPlayModeHandling";
    private HsmTarget target;
    private CarPlayGlobalProperies properties;
    private CarPlayAudiohandling audiohandling;
    private ASLNavigationServices navServices;
    private SpeechApplicant speechapplicant;
    private ExboxGuidanceListenerImpl exboxGuidanceListenerImpl;

    public CarPlayModeHandling(HsmTarget hsmTarget, CarPlayGlobalProperies carPlayGlobalProperies, CarPlayAudiohandling carPlayAudiohandling, ASLNavigationServices aSLNavigationServices, SpeechApplicant speechApplicant, ExboxGuidanceListenerImpl exboxGuidanceListenerImpl) {
        this.target = hsmTarget;
        this.properties = carPlayGlobalProperies;
        this.audiohandling = carPlayAudiohandling;
        this.navServices = aSLNavigationServices;
        this.speechapplicant = speechApplicant;
        this.exboxGuidanceListenerImpl = exboxGuidanceListenerImpl;
    }

    public int getMainScreenType(Resource[] resourceArray) {
        if (CarPlayHelper.containsElement((Object[])resourceArray, (Object)CarPlayModeParameter.resource_main_screen_device)) {
            return 2;
        }
        if (CarPlayHelper.containsElement((Object[])resourceArray, (Object)CarPlayModeParameter.resource_main_screen_hmi)) {
            return 1;
        }
        return 0;
    }

    public Resource getMainScreenResource(int n) {
        if (n == 2) {
            return CarPlayModeParameter.resource_main_screen_device;
        }
        if (n == 1) {
            return CarPlayModeParameter.resource_main_screen_hmi;
        }
        return CarPlayModeParameter.resource_main_screen_undefined;
    }

    public int getMainAudioSource(Resource[] resourceArray) {
        if (CarPlayHelper.containsElement((Object[])resourceArray, (Object)CarPlayModeParameter.resource_main_audio_device)) {
            return 2;
        }
        if (CarPlayHelper.containsElement((Object[])resourceArray, (Object)CarPlayModeParameter.resource_main_audio_hmi)) {
            return 1;
        }
        return 0;
    }

    public Resource getMainAudioResource(int n) {
        if (n == 2) {
            return CarPlayModeParameter.resource_main_audio_device;
        }
        if (n == 1) {
            return CarPlayModeParameter.resource_main_audio_hmi;
        }
        return CarPlayModeParameter.resource_main_audio_undefined;
    }

    public int getNavigationType(AppState[] appStateArray) {
        if (CarPlayHelper.containsElement((Object[])appStateArray, (Object)CarPlayModeParameter.appstate_nav_device)) {
            return 2;
        }
        if (CarPlayHelper.containsElement((Object[])appStateArray, (Object)CarPlayModeParameter.appstate_nav_hmi)) {
            return 3;
        }
        return 1;
    }

    public int getSpeechType(AppState[] appStateArray) {
        if (CarPlayHelper.containsElement((Object[])appStateArray, (Object)CarPlayModeParameter.appstate_speech_devide_recognizing) || CarPlayHelper.containsElement((Object[])appStateArray, (Object)CarPlayModeParameter.appstate_speech_devide_speak)) {
            return 2;
        }
        if (CarPlayHelper.containsElement((Object[])appStateArray, (Object)CarPlayModeParameter.appstate_speech_hmi)) {
            return 3;
        }
        return 1;
    }

    public AppState getAppstatePhone() {
        return CarPlayModeParameter.appstate_phonecall_unknown;
    }

    public AppState getAppstateNavigation(int n) {
        if (n == 2) {
            return CarPlayModeParameter.appstate_nav_device;
        }
        if (n == 3) {
            return CarPlayModeParameter.appstate_nav_hmi;
        }
        return CarPlayModeParameter.appstate_nav_unknown;
    }

    public AppState getAppstateSpeech() {
        return CarPlayModeParameter.appstate_speech_unknown;
    }

    public AppState getAppstateMedia() {
        return CarPlayModeParameter.appstate_media_unknown;
    }

    public static int getDSIAppstateRequest(AppState[] appStateArray) {
        boolean bl = CarPlayHelper.containsElement((Object[])appStateArray, (Object)CarPlayModeParameter.appstate_default);
        boolean bl2 = CarPlayHelper.containsElement((Object[])appStateArray, (Object)CarPlayModeParameter.appstate_media_device);
        if (bl2) {
            return 4;
        }
        if (bl) {
            return 0;
        }
        return -1;
    }

    public void handleModeChanges(Resource[] resourceArray, AppState[] appStateArray) {
        int n;
        int n2;
        if (this.target.isTraceEnabled()) {
            LogMessage logMessage = this.target.trace();
            logMessage.append(this.classname).append(".handledsiCarPlayupdateMode").log();
        }
        if ((n2 = this.getMainScreenType(resourceArray)) != this.properties.getCurrentScreenSource()) {
            if (n2 == 2) {
                if (this.properties.getCurrentScreenSource() != 0) {
                    if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), 100012)) {
                        this.properties.setDoEdgeDelayExecution(true);
                    } else {
                        n = 0;
                        if (!this.properties.isCanvasVisible()) {
                            if (this.target.isTraceEnabled()) {
                                this.target.trace("CARPLAY - trigger ASL_SMARTPHONEINTEGRATION_CARPLAY_TRIGGERED_CONTEXTSWITCH_TO_CARPLAY");
                            }
                            this.target.triggerObserver(6100008, null);
                            n = this.properties.isVideoBorrowState() ? 1 : 0;
                            this.properties.setVideoBorrowState(false);
                        }
                        if (this.properties.isCanvasDimed()) {
                            if (this.target.isTraceEnabled()) {
                                this.target.trace("CARPLAY - screen requested but a popup is still open -> borrow screen back");
                            }
                            if (n == 0) {
                                this.borrowScreen("Borrow Screen: Screen was requested but a popup is still open");
                            }
                        }
                    }
                }
            } else if (n2 == 1) {
                if (this.properties.getCurrentScreenSource() != 0) {
                    if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), 100012)) {
                        if (this.target.isTraceEnabled()) {
                            this.target.trace("CARPLAY - stop edge delay timer...");
                        }
                        this.target.stopTimer(100012);
                    }
                    if (this.properties.isCanvasVisible() && !this.properties.isVideoBorrowState()) {
                        if (this.target.isTraceEnabled()) {
                            this.target.trace("CARPLAY - trigger ASL_SMARTPHONEINTEGRATION_CARPLAY_TRIGGERED_CONTEXTSWITCH_LEFT_CARPLAY");
                        }
                        this.target.triggerObserver(6100012, null);
                    } else if (this.properties.isCanvasVisible() && this.target.isTraceEnabled()) {
                        this.target.trace("CARPLAY - ignore change to hmi because borrow state is active");
                    }
                }
            } else if (n2 == 0) {
                // empty if block
            }
            this.properties.setCurrentScreenSource(n2);
        }
        n = this.getNavigationType(appStateArray);
        int n3 = this.properties.getActiveNavigation();
        if (n == 2 && n3 != 2) {
            if (this.target.isTraceEnabled()) {
                this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Active Navigation is now: CARPLAY");
            }
            ServiceManager.aslPropertyManager.valueChangedBoolean(2910009, true);
            this.properties.setActiveNavigation(2);
            if (this.navServices.isServiceAvailable()) {
                this.navServices.stopGuidance(6);
            } else if (this.target.isTraceEnabled()) {
                this.target.trace("CARPLAY - handledsiCarPlayupdateMode - We can not stop hmi navi because service is not ready yet");
            }
            if (this.exboxGuidanceListenerImpl != null) {
                this.exboxGuidanceListenerImpl.setSmartphoneGuidanceActive();
            }
        } else if (n == 3 && n3 != 3) {
            if (this.target.isTraceEnabled()) {
                this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Active Navigation is now: HMI");
            }
            if (n3 == 2) {
                ServiceManager.aslPropertyManager.valueChangedBoolean(2910009, false);
            }
            this.properties.setActiveNavigation(3);
            if (!this.properties.isHmiRouteGuidanceActive()) {
                CarPlayHMIRequestParameterConfiguration carPlayHMIRequestParameterConfiguration = this.properties.getParamconfig();
                this.properties.getParamconfig().getClass();
                ResourceRequest[] resourceRequestArray = carPlayHMIRequestParameterConfiguration.getRessourceRequest(0);
                CarPlayHMIRequestParameterConfiguration carPlayHMIRequestParameterConfiguration2 = this.properties.getParamconfig();
                this.properties.getParamconfig().getClass();
                AppStateRequest[] appStateRequestArray = carPlayHMIRequestParameterConfiguration2.getAppStateRequest(2);
                this.target.getDSICarPlay().requestModeChange(resourceRequestArray, appStateRequestArray, "User initiated");
            }
            if (this.exboxGuidanceListenerImpl != null) {
                this.exboxGuidanceListenerImpl.setSmartphoneGuidanceInactive();
            }
        } else if (n == 1) {
            if (this.target.isTraceEnabled()) {
                this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Navigation is now inactive");
            }
            if (n3 == 2) {
                ServiceManager.aslPropertyManager.valueChangedBoolean(2910009, false);
            }
            this.properties.setActiveNavigation(1);
            if (this.exboxGuidanceListenerImpl != null) {
                this.exboxGuidanceListenerImpl.setSmartphoneGuidanceInactive();
            }
        }
        int n4 = this.properties.getCurrentMainAudioSource();
        int n5 = this.getMainAudioSource(resourceArray);
        this.properties.setCurrentMainAudioSource(n5);
        if (n4 == 2 && n5 != 2) {
            if (this.target.isTraceEnabled()) {
                this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Audio source is no longer device");
            }
            if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), 100011)) {
                this.target.stopTimer(100011);
            }
            if (!this.properties.isAudioBorrowState()) {
                this.audiohandling.releaseAllConnections(true, false);
                if (this.speechapplicant.isCarplayActive()) {
                    this.speechapplicant.forceChangeState(ActivationState.INACTIVE);
                }
                if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), 100009)) {
                    if (this.target.isTraceEnabled()) {
                        this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Detected a audio change to mainunit AND timer is still running => AUTOPLAY");
                    }
                    this.target.stopTimer(100009);
                    if (this.properties.getLastEntertainmentContextForAutoplay() != -1 && this.properties.getLastEntertainmentContextForAutoplay() != 7) {
                        EventGeneric eventGeneric = ServiceManager.mGenericEventFactory.newEvent(1400025);
                        eventGeneric.setInt(0, this.properties.getLastEntertainmentContextForAutoplay());
                        this.target.sendSafe(eventGeneric);
                    } else if (this.target.isTraceEnabled()) {
                        this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Can not execute autoplay because we dont know the previous context!");
                    }
                }
                this.properties.setAutoplayFirstMediaRequest(true);
                if (this.target.isTraceEnabled()) {
                    this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Set MainAudioType to NONE");
                }
                this.properties.setCurrentMainAudioType(0);
            } else if (this.target.isTraceEnabled()) {
                this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Ignore change because we are in the borrow state");
            }
        } else if (n4 != 2 && n5 == 2) {
            if (this.target.isTraceEnabled()) {
                this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Audio source is now device");
            }
            this.properties.setAudioBorrowState(false);
            this.audiohandling.handleAudioSourceChange(this.properties.getCurrentMainAudioType(), n4, false);
            this.properties.setAutoplayFirstMediaRequest(true);
        }
        int n6 = this.getSpeechType(appStateArray);
        this.properties.setActiveSpeech(n6);
        if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), 100010)) {
            this.target.stopTimer(100010);
        }
        this.target.startTimer(100010, 5000L, false);
    }

    public void borrowScreen(String string) {
        if (this.target.isTraceEnabled()) {
            this.target.trace("CARPLAY - send BORROW SCREEN request...");
        }
        CarPlayHMIRequestParameterConfiguration carPlayHMIRequestParameterConfiguration = this.properties.getParamconfig();
        this.properties.getParamconfig().getClass();
        ResourceRequest[] resourceRequestArray = carPlayHMIRequestParameterConfiguration.getRessourceRequest(9);
        CarPlayHMIRequestParameterConfiguration carPlayHMIRequestParameterConfiguration2 = this.properties.getParamconfig();
        this.properties.getParamconfig().getClass();
        AppStateRequest[] appStateRequestArray = carPlayHMIRequestParameterConfiguration2.getAppStateRequest(0);
        this.properties.setVideoBorrowState(true);
        this.target.getDSICarPlay().requestModeChange(resourceRequestArray, appStateRequestArray, string.equals("") ? "User initiated" : string);
    }

    public void unborrowScreen(String string) {
        if (this.target.isTraceEnabled()) {
            this.target.trace("CARPLAY - send UNBORROW SCREEN request...");
        }
        CarPlayHMIRequestParameterConfiguration carPlayHMIRequestParameterConfiguration = this.properties.getParamconfig();
        this.properties.getParamconfig().getClass();
        ResourceRequest[] resourceRequestArray = carPlayHMIRequestParameterConfiguration.getRessourceRequest(10);
        CarPlayHMIRequestParameterConfiguration carPlayHMIRequestParameterConfiguration2 = this.properties.getParamconfig();
        this.properties.getParamconfig().getClass();
        AppStateRequest[] appStateRequestArray = carPlayHMIRequestParameterConfiguration2.getAppStateRequest(0);
        this.properties.setVideoBorrowState(false);
        this.target.getDSICarPlay().requestModeChange(resourceRequestArray, appStateRequestArray, string.equals("") ? "User initiated" : string);
    }

    public void takeScreen(String string) {
        if (this.target.isTraceEnabled()) {
            this.target.trace("CARPLAY - send TAKE SCREEN request...");
        }
        if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), 100012)) {
            if (this.target.isTraceEnabled()) {
                this.target.trace("CARPLAY - no need for the edge delay timer because it is already running...");
            }
        } else {
            if (this.target.isTraceEnabled()) {
                this.target.trace("CARPLAY - starting edge delay timer...");
            }
            this.properties.setDoEdgeDelayExecution(false);
            this.target.startTimer(100012, 300L, false);
        }
        CarPlayHMIRequestParameterConfiguration carPlayHMIRequestParameterConfiguration = this.properties.getParamconfig();
        this.properties.getParamconfig().getClass();
        ResourceRequest[] resourceRequestArray = carPlayHMIRequestParameterConfiguration.getRessourceRequest(1);
        CarPlayHMIRequestParameterConfiguration carPlayHMIRequestParameterConfiguration2 = this.properties.getParamconfig();
        this.properties.getParamconfig().getClass();
        AppStateRequest[] appStateRequestArray = carPlayHMIRequestParameterConfiguration2.getAppStateRequest(0);
        this.properties.setVideoBorrowState(false);
        this.target.getDSICarPlay().requestModeChange(resourceRequestArray, appStateRequestArray, string.equals("") ? "User initiated" : string);
    }
}

