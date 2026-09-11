package de.vw.mib.asl.internal.androidauto.target;

import de.vw.mib.asl.framework.internal.framework.ServiceManager;
import de.vw.mib.asl.internal.androidauto.api.impl.ExboxGuidanceListenerImpl;
import de.vw.mib.genericevents.EventGeneric;

public class RequestHandler {
   AndroidAutoTarget target;
   private AudioHandler audioHandler;
   private NavigationListener navigationHandler;
   private AndroidAutoGlobalProperties properties;
   private SpeechHandler speechHandler;
   private StartupHandler startupHandler;
   private TimerHandler timerHandler;
   private ExboxGuidanceListenerImpl exboxGuidanceListenerImpl;

   public RequestHandler(AndroidAutoTarget var1, AndroidAutoGlobalProperties var2, StartupHandler var3, AudioHandler var4, SpeechHandler var5) {
      this.target = var1;
      this.properties = var2;
      this.startupHandler = var3;
      this.audioHandler = var4;
      this.speechHandler = var5;
      this.navigationHandler = null;
      this.timerHandler = null;
      this.exboxGuidanceListenerImpl = null;
   }

   public void initTimerHandler(TimerHandler var1) {
      this.timerHandler = var1;
   }

   public void initNavigationListener(NavigationListener var1) {
      this.navigationHandler = var1;
   }

   public void initExBoxNavServices(ExboxGuidanceListenerImpl var1) {
      this.exboxGuidanceListenerImpl = var1;
   }

   public void performVideoFocusRequestNotification(int var1) {
      if (this.target.isTraceEnabled()) {
         this.target.trace("RequestHandler::performVideoFocusRequestNotification - videoFocusMode = " + var1);
      }

      switch (var1) {
         case 1:
            if (this.properties.getVideoFocusState() == 2) {
               EventGeneric var3 = ServiceManager.mGenericEventFactory.newEvent(840195328);
               this.target.sendSafe(var3);
               this.properties.setVideoFocusState(1);
            } else if (this.target.isTraceEnabled()) {
               this.target.trace("=> VideoFocus is already projected...");
            }

            this.target.getDSIAndroidAuto2().videoFocusNotification(1, false);
            break;
         case 2:
            if (this.properties.getVideoFocusState() == 1) {
               EventGeneric var2 = ServiceManager.mGenericEventFactory.newEvent(856972544);
               this.target.sendSafe(var2);
               this.properties.setVideoFocusState(2);
            } else if (this.target.isTraceEnabled()) {
               this.target.trace("=> VideoFocus is already native...");
            }

            this.target.getDSIAndroidAuto2().videoFocusNotification(2, false);
            break;
         default:
            this.target.warn("VideoFocusMode " + var1 + " is invalid!");
      }
   }

   public void performVideoAvailable(boolean var1) {
      if (this.target.isTraceEnabled()) {
         this.target.trace("RequestHandler::performVideoAvailable - videoAvailable = " + var1);
      }

      if (var1) {
         this.properties.setVideoStreamAvailable(true);
         if (this.timerHandler.isVideoAvailableTimerStarted()) {
            this.timerHandler.stopVideoAvailableTimer();
         }

         ServiceManager.aslPropertyManager.valueChangedBoolean(845622272, this.properties.isVideoStreamAvailable());
      } else {
         this.properties.setVideoStreamAvailable(false);
      }
   }

   public void videoAvailableTimerExpired() {
      if (this.target.isTraceEnabled()) {
         this.target.trace("RequestHandler::videoAvailableTimerExpired");
      }

      ServiceManager.aslPropertyManager.valueChangedBoolean(845622272, false);
   }

   public void performAudioFocusRequestNotification(int var1) {
      if (this.target.isTraceEnabled()) {
         this.target.trace("RequestHandler::performAudioFocusRequestNotification - audioFocusRequestType = " + var1);
         this.target.trace("=====> properties.isUserMuteStarted() = " + this.properties.isUserMuteStarted());
         this.target.trace("=====> properties.isUserMuteActive() = " + this.properties.isUserMuteActive());
         this.target.trace("=====> properties.isAudible() = " + this.properties.isAudible());
      }

      switch (var1) {
         case 1:
            this.properties.setCurrentAudioFocusRequest(1);
            this.audioHandler.performAudioFocusRequestGain();
            break;
         case 2:
            this.properties.setCurrentAudioFocusRequest(2);
            this.audioHandler.performAudioFocusRequestGainTransient();
            break;
         case 3:
            this.properties.setCurrentAudioFocusRequest(3);
            this.audioHandler.performAudioFocusRequestGainTransientMayDuck();
            break;
         case 4:
            this.properties.setCurrentAudioFocusRequest(4);
            this.audioHandler.performAudioFocusRequestRelease();
            break;
         default:
            this.target.warn("AudioFocusRequestType " + var1 + " is invalid!");
      }
   }

   public void performVoiceSessionNotification(int var1) {
      if (this.target.isTraceEnabled()) {
         this.target.trace("RequestHandler::performVoiceSessionNotification - voiceSessionStatus = " + var1);
      }

      switch (var1) {
         case 1:
            if (this.target.isTraceEnabled()) {
               this.target.trace("===> forceStartGALSpeech()");
            }

            this.speechHandler.forceStartGALSpeech();
            break;
         case 2:
            if (this.target.isTraceEnabled()) {
               this.target.trace("===> forceStopGALSpeech()");
            }

            this.speechHandler.forceStopGALSpeech();
            break;
         default:
            this.target.warn("VoiceSessionStatus " + var1 + " is invalid!");
      }
   }

   public void voiceSessionStartDelayTimerExpired() {
      if (this.target.isTraceEnabled()) {
         this.target.trace("RequestHandler::voiceSessionStartDelayTimerExpired");
      }

      this.speechHandler.forceStartGALSpeech();
   }

   public void performMicrophoneRequestNotification(int var1) {
      if (this.target.isTraceEnabled()) {
         this.target.trace("RequestHandler::performMicrophoneRequestNotification - micModeStatus = " + var1);
      }

      switch (var1) {
         case 1:
            this.audioHandler.performMicrophoneRequestMicrophoneOpen();
            break;
         case 2:
            this.audioHandler.performMicrophoneRequestMicrophoneClose();
            break;
         default:
            this.target.warn("MicModeStatus " + var1 + " is invalid!");
      }
   }

   public void performNavFocusRequestNotification(int var1) {
      if (this.target.isTraceEnabled()) {
         this.target.trace("RequestHandler::performNavFocusRequestNotification - navFocusType = " + var1);
      }

      switch (var1) {
         case 1:
         case 2:
            if (this.navigationHandler.isGuidanceActive() && this.target.isTraceEnabled()) {
               this.target.trace("RequestHandler::performNavFocusRequestNotification ==> sending stopGuidance() to native navigation...");
            }

            this.properties.setAndroidAutoNavigationActive(true);
            if (this.startupHandler.isDeviceConnected() && this.startupHandler.isDSI2Registered()) {
               this.target.getDSIAndroidAuto2().navFocusNotification(2, false);
            }
            break;
         default:
            this.target.warn("NavFocusType " + var1 + " is invalid!");
      }
   }
}
