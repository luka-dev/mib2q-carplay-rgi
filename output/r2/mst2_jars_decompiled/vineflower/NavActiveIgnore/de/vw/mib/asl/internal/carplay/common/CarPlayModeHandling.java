package de.vw.mib.asl.internal.carplay.common;

import de.vw.mib.asl.api.navigation.ASLNavigationServices;
import de.vw.mib.asl.framework.internal.framework.ServiceManager;
import de.vw.mib.asl.internal.carplay.audio.CarPlayAudiohandling;
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

   public CarPlayModeHandling(
      HsmTarget var1, CarPlayGlobalProperies var2, CarPlayAudiohandling var3, ASLNavigationServices var4, SpeechApplicant var5, ExboxGuidanceListenerImpl var6
   ) {
      this.target = var1;
      this.properties = var2;
      this.audiohandling = var3;
      this.navServices = var4;
      this.speechapplicant = var5;
      this.exboxGuidanceListenerImpl = var6;
   }

   public int getMainScreenType(Resource[] var1) {
      if (CarPlayHelper.containsElement(var1, CarPlayModeParameter.resource_main_screen_device)) {
         return 2;
      } else {
         return CarPlayHelper.containsElement(var1, CarPlayModeParameter.resource_main_screen_hmi) ? 1 : 0;
      }
   }

   public Resource getMainScreenResource(int var1) {
      if (var1 == 2) {
         return CarPlayModeParameter.resource_main_screen_device;
      } else {
         return var1 == 1 ? CarPlayModeParameter.resource_main_screen_hmi : CarPlayModeParameter.resource_main_screen_undefined;
      }
   }

   public int getMainAudioSource(Resource[] var1) {
      if (CarPlayHelper.containsElement(var1, CarPlayModeParameter.resource_main_audio_device)) {
         return 2;
      } else {
         return CarPlayHelper.containsElement(var1, CarPlayModeParameter.resource_main_audio_hmi) ? 1 : 0;
      }
   }

   public Resource getMainAudioResource(int var1) {
      if (var1 == 2) {
         return CarPlayModeParameter.resource_main_audio_device;
      } else {
         return var1 == 1 ? CarPlayModeParameter.resource_main_audio_hmi : CarPlayModeParameter.resource_main_audio_undefined;
      }
   }

   public int getNavigationType(AppState[] var1) {
      if (CarPlayHelper.containsElement(var1, CarPlayModeParameter.appstate_nav_device)) {
         return 2;
      } else {
         return CarPlayHelper.containsElement(var1, CarPlayModeParameter.appstate_nav_hmi) ? 3 : 1;
      }
   }

   public int getSpeechType(AppState[] var1) {
      if (CarPlayHelper.containsElement(var1, CarPlayModeParameter.appstate_speech_devide_recognizing)
         || CarPlayHelper.containsElement(var1, CarPlayModeParameter.appstate_speech_devide_speak)) {
         return 2;
      } else {
         return CarPlayHelper.containsElement(var1, CarPlayModeParameter.appstate_speech_hmi) ? 3 : 1;
      }
   }

   public AppState getAppstatePhone() {
      return CarPlayModeParameter.appstate_phonecall_unknown;
   }

   public AppState getAppstateNavigation(int var1) {
      if (var1 == 2) {
         return CarPlayModeParameter.appstate_nav_device;
      } else {
         return var1 == 3 ? CarPlayModeParameter.appstate_nav_hmi : CarPlayModeParameter.appstate_nav_unknown;
      }
   }

   public AppState getAppstateSpeech() {
      return CarPlayModeParameter.appstate_speech_unknown;
   }

   public AppState getAppstateMedia() {
      return CarPlayModeParameter.appstate_media_unknown;
   }

   public static int getDSIAppstateRequest(AppState[] var0) {
      boolean var1 = CarPlayHelper.containsElement(var0, CarPlayModeParameter.appstate_default);
      boolean var2 = CarPlayHelper.containsElement(var0, CarPlayModeParameter.appstate_media_device);
      if (var2) {
         return 4;
      } else {
         return var1 ? 0 : -1;
      }
   }

   public void handleModeChanges(Resource[] var1, AppState[] var2) {
      if (this.target.isTraceEnabled()) {
         LogMessage var5 = this.target.trace();
         var5.append(this.classname).append(".handledsiCarPlayupdateMode").log();
      }

      int var4;
      if ((var4 = this.getMainScreenType(var1)) != this.properties.getCurrentScreenSource()) {
         if (var4 == 2) {
            if (this.properties.getCurrentScreenSource() != 0) {
               if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), -1400504064)) {
                  this.properties.setDoEdgeDelayExecution(true);
               } else {
                  boolean var3 = false;
                  if (!this.properties.isCanvasVisible()) {
                     if (this.target.isTraceEnabled()) {
                        this.target.trace("CARPLAY - trigger ASL_SMARTPHONEINTEGRATION_CARPLAY_TRIGGERED_CONTEXTSWITCH_TO_CARPLAY");
                     }

                     this.target.triggerObserver(672423168, null);
                     var3 = this.properties.isVideoBorrowState();
                     this.properties.setVideoBorrowState(false);
                  }

                  if (this.properties.isCanvasDimed()) {
                     if (this.target.isTraceEnabled()) {
                        this.target.trace("CARPLAY - screen requested but a popup is still open -> borrow screen back");
                     }

                     if (!var3) {
                        this.borrowScreen("Borrow Screen: Screen was requested but a popup is still open");
                     }
                  }
               }
            }
         } else if (var4 == 1) {
            if (this.properties.getCurrentScreenSource() != 0) {
               if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), -1400504064)) {
                  if (this.target.isTraceEnabled()) {
                     this.target.trace("CARPLAY - stop edge delay timer...");
                  }

                  this.target.stopTimer(-1400504064);
               }

               if (this.properties.isCanvasVisible() && !this.properties.isVideoBorrowState()) {
                  if (this.target.isTraceEnabled()) {
                     this.target.trace("CARPLAY - trigger ASL_SMARTPHONEINTEGRATION_CARPLAY_TRIGGERED_CONTEXTSWITCH_LEFT_CARPLAY");
                  }

                  this.target.triggerObserver(739532032, null);
               } else if (this.properties.isCanvasVisible() && this.target.isTraceEnabled()) {
                  this.target.trace("CARPLAY - ignore change to hmi because borrow state is active");
               }
            }
         } else if (var4 == 0) {
         }

         this.properties.setCurrentScreenSource(var4);
      }

      int var9 = this.getNavigationType(var2);
      int var10 = this.properties.getActiveNavigation();
      System.out.println("CarPlayModeHandling(" + var1 + " (" + var4 + "), " + var2 + "(" + var9 + ")): " + var10);
      if (var9 == 2 && var10 != 2) {
         this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Active Navigation is now: CARPLAY");
         if (!this.navServices.isServiceAvailable() && this.target.isTraceEnabled()) {
            this.target.trace("CARPLAY - handledsiCarPlayupdateMode - We can not stop hmi navi because service is not ready yet");
         }
      } else if (var9 == 3 && var10 != 3) {
         this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Active Navigation is now: HMI");
         if (var10 == 2) {
            ServiceManager.aslPropertyManager.valueChangedBoolean(963062784, false);
         }

         this.properties.setActiveNavigation(3);
      } else if (var9 == 1) {
         this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Navigation is now inactive");
         if (var10 == 2) {
            ServiceManager.aslPropertyManager.valueChangedBoolean(963062784, false);
         }

         this.properties.setActiveNavigation(1);
      }

      int var6 = this.properties.getCurrentMainAudioSource();
      int var7 = this.getMainAudioSource(var1);
      this.properties.setCurrentMainAudioSource(var7);
      if (var6 == 2 && var7 != 2) {
         if (this.target.isTraceEnabled()) {
            this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Audio source is no longer device");
         }

         if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), -1417281280)) {
            this.target.stopTimer(-1417281280);
         }

         if (!this.properties.isAudioBorrowState()) {
            this.audiohandling.releaseAllConnections(true, false);
            if (this.speechapplicant.isCarplayActive()) {
               this.speechapplicant.forceChangeState(ActivationState.INACTIVE);
            }

            if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), -1450835712)) {
               if (this.target.isTraceEnabled()) {
                  this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Detected a audio change to mainunit AND timer is still running => AUTOPLAY");
               }

               this.target.stopTimer(-1450835712);
               if (this.properties.getLastEntertainmentContextForAutoplay() != -1 && this.properties.getLastEntertainmentContextForAutoplay() != 7) {
                  EventGeneric var8 = ServiceManager.mGenericEventFactory.newEvent(-648276736);
                  var8.setInt(0, this.properties.getLastEntertainmentContextForAutoplay());
                  this.target.sendSafe(var8);
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
      } else if (var6 != 2 && var7 == 2) {
         if (this.target.isTraceEnabled()) {
            this.target.trace("CARPLAY - handledsiCarPlayupdateMode - Audio source is now device");
         }

         this.properties.setAudioBorrowState(false);
         this.audiohandling.handleAudioSourceChange(this.properties.getCurrentMainAudioType(), var6, false);
         this.properties.setAutoplayFirstMediaRequest(true);
      }

      int var11 = this.getSpeechType(var2);
      this.properties.setActiveSpeech(var11);
      if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), -1434058496)) {
         this.target.stopTimer(-1434058496);
      }

      this.target.startTimer(-1434058496, 0L, false);
   }

   public void borrowScreen(String var1) {
      if (this.target.isTraceEnabled()) {
         this.target.trace("CARPLAY - send BORROW SCREEN request...");
      }

      CarPlayHMIRequestParameterConfiguration var2 = this.properties.getParamconfig();
      super.getClass();
      ResourceRequest[] var3 = var2.getRessourceRequest(9);
      CarPlayHMIRequestParameterConfiguration var4 = this.properties.getParamconfig();
      super.getClass();
      AppStateRequest[] var5 = var4.getAppStateRequest(0);
      this.properties.setVideoBorrowState(true);
      this.target.getDSICarPlay().requestModeChange(var3, var5, var1.equals("") ? "User initiated" : var1);
   }

   public void unborrowScreen(String var1) {
      if (this.target.isTraceEnabled()) {
         this.target.trace("CARPLAY - send UNBORROW SCREEN request...");
      }

      CarPlayHMIRequestParameterConfiguration var2 = this.properties.getParamconfig();
      super.getClass();
      ResourceRequest[] var3 = var2.getRessourceRequest(10);
      CarPlayHMIRequestParameterConfiguration var4 = this.properties.getParamconfig();
      super.getClass();
      AppStateRequest[] var5 = var4.getAppStateRequest(0);
      this.properties.setVideoBorrowState(false);
      this.target.getDSICarPlay().requestModeChange(var3, var5, var1.equals("") ? "User initiated" : var1);
   }

   public void takeScreen(String var1) {
      if (this.target.isTraceEnabled()) {
         this.target.trace("CARPLAY - send TAKE SCREEN request...");
      }

      if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), -1400504064)) {
         if (this.target.isTraceEnabled()) {
            this.target.trace("CARPLAY - no need for the edge delay timer because it is already running...");
         }
      } else {
         if (this.target.isTraceEnabled()) {
            this.target.trace("CARPLAY - starting edge delay timer...");
         }

         this.properties.setDoEdgeDelayExecution(false);
         this.target.startTimer(-1400504064, 0L, false);
      }

      CarPlayHMIRequestParameterConfiguration var2 = this.properties.getParamconfig();
      super.getClass();
      ResourceRequest[] var3 = var2.getRessourceRequest(1);
      CarPlayHMIRequestParameterConfiguration var4 = this.properties.getParamconfig();
      super.getClass();
      AppStateRequest[] var5 = var4.getAppStateRequest(0);
      this.properties.setVideoBorrowState(false);
      this.target.getDSICarPlay().requestModeChange(var3, var5, var1.equals("") ? "User initiated" : var1);
   }
}
