package de.vw.mib.asl.internal.carplay.target;

import de.vw.mib.asl.api.smartphoneintegration.ASLSmartphoneIntegrationAPI;
import de.vw.mib.asl.api.smartphoneintegration.ASLSmartphoneIntegrationFactory;
import de.vw.mib.asl.api.system.ASLSystemFactory;
import de.vw.mib.asl.framework.internal.framework.ServiceManager;
import de.vw.mib.asl.internal.carplay.audio.CarPlayAudiohandling;
import de.vw.mib.asl.internal.carplay.common.CarPlayGlobalProperies;
import de.vw.mib.asl.internal.carplay.common.CarPlayHMIRequestParameterConfiguration;
import de.vw.mib.asl.internal.carplay.common.CarPlayModeHandling;
import de.vw.mib.asl.internal.carplay.speech.SpeechApplicant;
import de.vw.mib.dialogactivation.ActivationState;
import de.vw.mib.genericevents.EventGeneric;
import generated.de.vw.mib.asl.internal.ListManager;
import generated.de.vw.mib.asl.internal.carplay.transformer.CarPlayCanvasDimensionCollector;
import java.util.Hashtable;
import org.dsi.ifc.carplay.AppStateRequest;
import org.dsi.ifc.carplay.ResourceRequest;
import org.dsi.ifc.carplay.ServiceConfiguration;
import org.dsi.ifc.carplay.TouchEvent;

public class ASLHandler {
   private CarPlayGlobalProperies properties;
   private HsmTarget target;
   private SpeechApplicant speechapplicant;
   private CarPlayAudiohandling audiohandling;
   private CarPlayModeHandling modehandling;
   private SpeechApplicant speech;
   public String classname = "ASLController";
   int TOUCH_EVENT_RELEASED = 0;
   int TOUCH_EVENT_PRESSED_ONE_FINGER = 1;
   int TOUCH_EVENT_PRESSED_TWO_FINGERS = 2;

   public ASLHandler(
      HsmTarget var1, CarPlayGlobalProperies var2, SpeechApplicant var3, CarPlayAudiohandling var4, CarPlayModeHandling var5, SpeechApplicant var6
   ) {
      this.target = var1;
      this.properties = var2;
      this.speechapplicant = var3;
      this.audiohandling = var4;
      this.modehandling = var5;
      this.speech = var6;
   }

   public void handleAslApiEnterCarPlayCanvas(EventGeneric var1) {
      int var2 = var1.getInt(0);
      this.properties.setCanvasVisible(true);
      if (this.properties.getConnectionState() != 0 && this.properties.getConnectionState() != 1 && this.properties.getCurrentScreenSource() != 2) {
         if (var2 == 6) {
            this.target.getDSICarPlay().requestUI(2);
         } else {
            this.target.getDSICarPlay().requestUI(0);
         }
      }
   }

   public void handleAslApiLeaveCarPlayCanvas() {
      this.properties.setCanvasVisible(false);
      if (this.properties.getConnectionState() != 0 && this.properties.getConnectionState() != 1) {
         if (this.properties.getCurrentScreenSource() == 2 || this.properties.isVideoBorrowState()) {
            this.modehandling.takeScreen("Take Screen: Carplay canvas was left");
            if (1 != this.properties.getCurrentScreenSource()) {
               this.properties.setCurrentScreenSource(1);
            }
         }
      }
   }

   public void handleAslApiEV_CARPLAY_SOUND_SET_CONNECTION_ANSWER(int var1, int var2) {
      int var3 = this.properties.getCurrentSoundStatus(var1);
      int var4 = -1;
      short var5 = -1;
      if (var2 == 0 && var3 == 0 && this.target.isTraceEnabled()) {
         this.target
            .trace()
            .append(this.classname)
            .append(".")
            .append(
               "handleAslApiEV_CARPLAY_SOUND_SET_CONNECTION_ANSWER - WARNING! Got an answer out of order from audio management - got a result_ok for not requested connection "
                  + var1
            )
            .log();
      }

      if (var1 == 151 && this.properties.getCurrentMainAudioType() != 1) {
         var5 = 163;
      } else if (var1 == 163) {
         var5 = 151;
      } else if (var1 == 152) {
         var5 = 153;
      } else if (var1 == 153) {
         var5 = 152;
      }

      if (var5 != -1) {
         var4 = this.properties.getCurrentSoundStatus(var5);
      }

      if (var2 == 0) {
         this.properties.setCurrentSoundStatus(var1, 3);
         if (var4 != -1) {
            if (var4 == 3) {
               if (var3 != 3) {
                  if (this.target.isTraceEnabled()) {
                     this.target
                        .trace()
                        .append(this.classname)
                        .append(".")
                        .append("handleAslApiEV_CARPLAY_SOUND_SET_CONNECTION_ANSWER - result ok, is pair, pair status = ready, own was not ready (pair: ")
                        .append(var1)
                        .append(" and ")
                        .append(var5)
                        .append(") => response")
                        .log();
                  }

                  this.properties.setPhoneWaitsForResponseMainAudioType(false);
                  this.target.getDSICarPlay().responseUpdateMainAudioType(this.getAnswerForMainAudioType(var1));
               }
            } else if (var4 != 2 && var4 == 0) {
            }
         } else if (var1 != 154 && var1 != 155 && var3 != 3) {
            if (this.target.isTraceEnabled()) {
               this.target
                  .trace()
                  .append(this.classname)
                  .append(".")
                  .append("handleAslApiEV_CARPLAY_SOUND_SET_CONNECTION_ANSWER - result ok, no pair, own was not ready (channel: ")
                  .append(var1)
                  .append(") => response")
                  .log();
            }

            if (this.properties.isConnected()) {
               this.properties.setPhoneWaitsForResponseMainAudioType(false);
               this.target.getDSICarPlay().responseUpdateMainAudioType(this.getAnswerForMainAudioType(var1));
            }
         }
      } else {
         this.properties.setCurrentSoundStatus(var1, 0);
         if (var4 != -1) {
            if (var4 != 0 && (var4 == 2 || var4 == 3)) {
               if (this.target.isTraceEnabled()) {
                  this.target
                     .trace()
                     .append(this.classname)
                     .append(".")
                     .append("handleAslApiEV_CARPLAY_SOUND_SET_CONNECTION_ANSWER - result Nok, is pair, pair status = ready|pending (pair: ")
                     .append(var1)
                     .append(" and ")
                     .append(var5)
                     .append(") => response & take audio")
                     .log();
               }

               this.properties.setPhoneWaitsForResponseMainAudioType(false);
               this.target.getDSICarPlay().responseUpdateMainAudioType(this.getAnswerForMainAudioType(var1));
               if (this.isValidConnection(var1)) {
                  this.audiohandling.releaseAudioConnection(var5, false);
                  this.audiohandling.takeMainAudio("Take Audio: Audio connection could not be established for one out of two requested connections");
               }
            }
         } else if (var1 != 154 && var1 != 155) {
            if (this.target.isTraceEnabled()) {
               this.target
                  .trace()
                  .append(this.classname)
                  .append(".")
                  .append("handleAslApiEV_CARPLAY_SOUND_SET_CONNECTION_ANSWER - result Nok, no pair (channel: ")
                  .append(var1)
                  .append(") => response & take audio")
                  .log();
            }

            if (this.properties.isConnected()) {
               this.properties.setPhoneWaitsForResponseMainAudioType(false);
               this.target.getDSICarPlay().responseUpdateMainAudioType(this.getAnswerForMainAudioType(var1));
               if (this.isValidConnection(var1)) {
                  this.audiohandling.takeMainAudio("Take Audio: Audio connection could not be established");
               }
            }
         }
      }

      if (var1 == 162 && var2 == 0) {
         if (!this.properties.isActiveEntertainment()) {
            if (this.target.isTraceEnabled()) {
               this.target
                  .trace()
                  .append(this.classname)
                  .append(".")
                  .append(
                     "handleAslApiEV_CARPLAY_SOUND_SET_CONNECTION_ANSWER - got the media connection but we are not active entertainment => release connection!"
                  )
                  .log();
            }

            this.audiohandling.releaseAudioConnection(162, true);
            return;
         }

         if (this.target.isTraceEnabled()) {
            this.target.trace().append(this.classname).append(".").append("handleAslApiEV_CARPLAY_SOUND_SET_CONNECTION_ANSWER - send ACTIVATED").log();
         }

         this.target.sendSafe(ServiceManager.mGenericEventFactory.newEvent(1400000));
         this.target.triggerObserver(6100023, null);
         if (this.properties.isConnected() && this.properties.getCurrentMainAudioSource() == 2) {
            this.audiohandling.changeVolumeLock(false);
            if (this.properties.getCurrentMuteState()) {
               this.audiohandling.releaseUserMute();
            }
         } else {
            this.audiohandling.changeVolumeLock(true);
         }

         this.audiohandling.checkAudibleStatus(this.properties.getCurrentAudibleState());
      }
   }

   public void handleAslApiEV_CARPLAY_SOUND_RELEASE_CONNECTION_ANSWER(int var1) {
      if (this.properties.getCurrentSoundStatus(var1) != 2 && this.properties.getCurrentSoundStatus(var1) != 1) {
         this.properties.setCurrentSoundStatus(var1, 0);
      }

      if ((var1 == 152 || var1 == 153) && this.speechapplicant.isCarplayActive()) {
         this.speechapplicant.forceChangeState(ActivationState.INACTIVE);
         this.target.getDSICarPlay().requestSIRIAction(1);
         this.target.getDSICarPlay().requestSIRIAction(2);
      }

      if (this.isValidConnection(var1) && this.properties.isSoundManagementAvailable() && this.properties.getCurrentSoundStatus(var1) != 1) {
         this.audiohandling.takeMainAudio("Take Audio: audio connection was released");
      }
   }

   public void handleAslApiASL_SMARTPHONEINTEGRATION_CONNECTION_ESTABLISHED_CARPLAY() {
      if (this.target.isTraceEnabled()) {
         this.target.trace().append(this.classname).append(".").append("handleAslApiASL_SMARTPHONEINTEGRATION_CONNECTION_ESTABLISHED_CARPLAY").log();
      }

      if (this.properties.getConnectionState() == 0) {
         this.properties.setConnectionState(1);
      } else {
         if (this.properties.getConnectionState() == 2) {
            if (!this.speechapplicant.isRegistered()) {
               if (this.target.isTraceEnabled()) {
                  this.target.trace().append(this.classname).append(".").append(" call speechapplicant.registerService()").log();
               }

               this.speechapplicant.registerService();
            }

            this.properties.setConnectionState(3);
            this.audiohandling.setAudioConnection(154, false);
            this.sendStartService();
         }
      }
   }

   public void handleAslApiASL_SMARTPHONEINTEGRATION_CONNECTION_LOST_CARPLAY() {
      if (this.target.isTraceEnabled()) {
         this.target.trace().append(this.classname).append(".").append("handleAslApiASL_SMARTPHONEINTEGRATION_CONNECTION_LOST_CARPLAY").log();
      }

      this.handleAslApiLeaveCarPlayCanvas();
      if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), 100011)) {
         this.target.stopTimer(100011);
      }

      if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), 100012)) {
         this.target.stopTimer(100012);
      }

      if (this.properties.getConnectionState() == 3) {
         this.properties.setConnectionState(2);
      } else if (this.properties.getConnectionState() == 1) {
         this.properties.setConnectionState(0);
      }

      this.audiohandling.releaseAllConnections(true, true);
      if (this.target.getTimerServer().isTimerActive(this.target.getTargetId(), 100010)) {
         this.target.stopTimer(100010);
      }

      this.speechapplicant.setWasSiriStart(false);
      if (this.speechapplicant.isCarplayActive()) {
         this.speechapplicant.forceChangeState(ActivationState.INACTIVE);
      }

      if (this.speechapplicant.isRegistered()) {
         this.speechapplicant.unregister();
      }

      if (this.properties.getActiveNavigation() == 2) {
         ServiceManager.aslPropertyManager.valueChangedBoolean(2910009, false);
         this.properties.setActiveNavigation(1);
      }

      this.properties.setAudioBorrowState(false);
      this.properties.setVideoBorrowState(false);
      this.properties.setCurrentMainAudioType(0);
      this.properties.setTelephonyStateValid(false);
      if (this.properties.isActiveEntertainment() && this.properties.getCurrentSoundStatus(162) == 3) {
         this.audiohandling.changeVolumeLock(true);
      }

      this.properties.setAutoplayFirstMediaRequest(true);
      this.properties.setDelayBorrowAudio(false);
      this.properties.setPhoneWaitsForResponseMainAudioType(false);
      this.properties.setCurrentCallStates(new Hashtable());
      this.target.getClusterService().sendCallStatesUpdate();
   }

   public void handleHmiSimpleTouchEvent(EventGeneric var1) {
      int var2 = var1.getInt(0);
      int var3 = var1.getInt(1);
      int var4 = var1.getInt(2);
      int var5 = var1.getInt(3);
      if (var4 == 0) {
         if (this.target.isTraceEnabled()) {
            this.target.trace().append(this.classname).append(".handleHmiSimpleTouchEvent (x,y)=(").append(var2).append(",").append(var3).append(")").log();
         }

         this.target
            .getDSICarPlay()
            .postTouchEvent(1, var5 == 0 ? this.TOUCH_EVENT_PRESSED_ONE_FINGER : this.TOUCH_EVENT_RELEASED, new TouchEvent[]{new TouchEvent(var2, var3)});
      }
   }

   public void handleHmiRotaryEvent(EventGeneric var1) {
      int var2 = var1.getInt(0);
      if (var2 == 0) {
         this.target.getDSICarPlay().postRotaryEvent(-1);
      } else {
         if (var2 == 1) {
            this.target.getDSICarPlay().postRotaryEvent(1);
         } else if (var2 == 2) {
            this.target.getDSICarPlay().postButtonEvent(1, 0);
         } else if (var2 == 4) {
            this.target.getDSICarPlay().postButtonEvent(1, 1);
         }
      }
   }

   public void handleHmiMflEvent(EventGeneric var1) {
      int var2 = var1.getInt(0);
      if (this.target.isTraceEnabled()) {
         this.target.trace().append(this.classname).append(".handleHmiMflEvent(gesture=").append(var2).log();
      }

      switch (var2) {
         case 0:
            this.target.getDSICarPlay().postButtonEvent(9, 1);
         case 1:
         case 5:
         case 6:
         case 7:
         default:
            break;
         case 2:
            this.target.getDSICarPlay().postButtonEvent(10, 1);
            break;
         case 3:
            this.target.getDSICarPlay().postButtonEvent(10, 1);
            break;
         case 4:
            this.target.getDSICarPlay().postButtonEvent(9, 1);
            break;
         case 8:
            this.target.getDSICarPlay().postButtonEvent(20, 0);
            break;
         case 9:
         case 10:
            this.target.getDSICarPlay().postButtonEvent(20, 1);
            break;
         case 11:
            this.target.getDSICarPlay().postButtonEvent(15, 0);
            break;
         case 12:
            this.target.getDSICarPlay().postButtonEvent(15, 1);
            break;
         case 13:
            this.target.getDSICarPlay().postButtonEvent(9, 0);
            break;
         case 14:
            this.target.getDSICarPlay().postButtonEvent(10, 0);
      }
   }

   public void sendStartService() {
      CarPlayHMIRequestParameterConfiguration var1 = this.properties.getParamconfig();
      String[] var2 = new String[0];
      boolean var3 = false;
      if (ServiceManager.configManagerDiag.getDriverSide() == 2) {
         var3 = true;
      }

      int var4 = ServiceManager.configManagerDiag.getCarplayCanvasOffsetX();
      int var5 = ServiceManager.configManagerDiag.getLayoutCarplayCanvasDimensionHeight();
      int var6 = ServiceManager.configManagerDiag.getLayoutCarplayCanvasDimensionWidth();
      int var7 = 105;
      int var8 = 174;
      ASLSmartphoneIntegrationAPI var9 = ASLSmartphoneIntegrationFactory.getSmartphoneIntegrationApi();
      if (var9 != null) {
         var8 = (int)(var9.getPixelDimensionWidth() * var6);
         var7 = (int)(var9.getPixelDimensionHeight() * var5);
         if (this.target.isTraceEnabled()) {
            this.target.trace("ASLHandler::sendStartService( width->" + var8 + "=" + var9.getPixelDimensionWidth() + "*" + var6);
            this.target.trace("ASLHandler::sendStartService( height->" + var7 + "=" + var9.getPixelDimensionHeight() + "*" + var5);
         }
      } else if (this.target.isTraceEnabled()) {
         this.target.trace("StartupHandler::startAndroidAutoService2(smartphoneintegrationAPI == null)");
      }

      CarPlayCanvasDimensionCollector[] var10 = new CarPlayCanvasDimensionCollector[]{new CarPlayCanvasDimensionCollector()};
      var10[0].carplay_height = var5;
      var10[0].carplay_width = var6;
      ListManager.getGenericASLList(2910008).updateList(var10);
      AppStateRequest[] var11 = var1.getCompleteAppStateRequest(
         false, this.properties.isHmiRouteGuidanceActive(), this.speech.isOtherSpeechActive() == ActivationState.ACTIVE
      );
      super.getClass();
      this.target
         .getDSICarPlay()
         .startService(
            new ServiceConfiguration(
               var11,
               var1.getRessourceRequest(3),
               3,
               var6,
               var5,
               var4,
               0,
               this.properties.getOemIconTitle(),
               var3,
               var6,
               var5,
               var2,
               this.properties.isNightMode(),
               var7,
               var8,
               this.getInputFeatures()
            )
         );
      this.properties.setCurrentScreenSource(1);
   }

   private int getInputFeatures() {
      return ASLSystemFactory.getSystemApi().getAbtFeatureService().getRotaryEncoderType() == 2 ? 5 : 4;
   }

   public void handleSoundManagerStatus(EventGeneric var1) {
      int var2 = var1.getInt(0);
      if (var2 == 0) {
         if (this.target.isTraceEnabled()) {
            this.target.trace().append(this.classname).append(".handle(ASL_SOUND_AUDIOMNGMNT_STATUS)").append(" ASL_SOUND_AUDIOMNGMNT_UNAVAILABLE").log();
         }

         this.properties.setSoundManagementAvailable(false);
         if (this.properties.isConnected()) {
            if (this.target.isTraceEnabled()) {
               this.target
                  .trace()
                  .append(this.classname)
                  .append(".handle(ASL_SOUND_AUDIOMNGMNT_STATUS)")
                  .append(" release all connections but leave EntertainmentManager alone")
                  .log();
            }

            this.audiohandling.releaseAllConnections(true, true);
            this.properties.setCurrentSoundStatus(162, 0);
            if (this.properties.getCurrentMainAudioSource() == 2) {
               if (this.target.isTraceEnabled()) {
                  this.target.trace().append(this.classname).append(".handle(ASL_SOUND_AUDIOMNGMNT_STATUS)").append(" borrow audio").log();
               }

               this.audiohandling.borrowMainAudio("Borrow audio: audio management is not longer available");
            }
         }
      } else if (var2 == 1) {
         if (this.target.isTraceEnabled()) {
            this.target.trace().append(this.classname).append(".handle(ASL_SOUND_AUDIOMNGMNT_STATUS)").append(" ASL_SOUND_AUDIOMNGMNT_AVAILABLE").log();
         }

         this.properties.setSoundManagementAvailable(true);
      } else {
         if (var2 == 2) {
            if (this.target.isTraceEnabled()) {
               this.target
                  .trace()
                  .append(this.classname)
                  .append(".handle(ASL_SOUND_AUDIOMNGMNT_STATUS)")
                  .append(" ASL_SOUND_AUDIOMNGMNT_AVAILABLE_RESTORE_CONNECTIONS")
                  .log();
            }

            this.properties.setSoundManagementAvailable(true);
            if (this.properties.isConnected()) {
               if (this.target.isTraceEnabled()) {
                  this.target.trace().append(this.classname).append(".handle(ASL_SOUND_AUDIOMNGMNT_STATUS)").append(" reconnect dio announcement").log();
               }

               this.audiohandling.setAudioConnection(154, false);
               if (this.properties.isActiveEntertainment()) {
                  if (this.target.isTraceEnabled()) {
                     this.target.trace().append(this.classname).append(".handle(ASL_SOUND_AUDIOMNGMNT_STATUS)").append(" unborrow main audio").log();
                  }

                  this.audiohandling.unborrowMainAudio("Unborrow Audio: audio management is now available again");
               }
            }
         }
      }
   }

   public void handleDayNightMode(EventGeneric var1) {
      boolean var2 = var1.getBoolean(0);
      if (this.target.isTraceEnabled()) {
         this.target.trace().append(this.classname).append(".handle(DAYNIGHT_MODE)").append(" new mode: night mode is ").append(var2).log();
      }

      this.properties.setNightMode(var2);
      if (this.properties.isConnected()) {
         this.target.getDSICarPlay().requestNightMode(var2);
      }
   }

   private boolean isValidConnection(int var1) {
      if ((var1 == 151 || var1 == 163) && this.properties.getCurrentMainAudioType() == 3) {
         return true;
      } else if ((var1 == 152 || var1 == 153) && this.properties.getCurrentMainAudioType() == 4) {
         return true;
      } else {
         return var1 == this.properties.getRingtoneConnection() && this.properties.getCurrentMainAudioType() == 1
            ? true
            : var1 == 162 && this.properties.getCurrentMainAudioType() == 2;
      }
   }

   private int getAnswerForMainAudioType(int var1) {
      if (var1 == 163) {
         return 3;
      }

      if (var1 == 162) {
         return 2;
      }

      if (var1 != 152 && var1 != 153) {
         if (var1 == this.properties.getRingtoneConnection() && this.properties.getRingtoneConnection() != 151) {
            return 1;
         } else if (var1 == 151 && this.properties.getRingtoneConnection() == 165) {
            return this.properties.getCurrentMainAudioType() == 1 ? 1 : 3;
         } else {
            return 0;
         }
      } else {
         return 4;
      }
   }

   public void checkDelayedBorrowState(int var1) {
      if (this.target.isTraceEnabled()) {
         this.target.trace().append(this.classname).append(".checkDelayedBorrowState - SDS ended").log();
      }

      if (this.properties.isDelayBorrowAudio() && this.properties.getCurrentMainAudioSource() == 2 && this.properties.getCurrentMainAudioType() == 2) {
         if (this.target.isTraceEnabled()) {
            this.target.trace().append(this.classname).append(".checkDelayedBorrowState - SDS warmup ended without sds session => do BORROW now").log();
         }

         if (this.properties.getCurrentSoundStatus(151) == 0
            && this.properties.getCurrentSoundStatus(163) == 0
            && this.properties.getCurrentSoundStatus(152) == 0
            && this.properties.getCurrentSoundStatus(153) == 0
            && this.properties.getCurrentSoundStatus(this.properties.getRingtoneConnection()) == 0) {
            this.audiohandling.borrowMainAudio("Borrow audio after delay: Audio is not audible but overlain by another audio source");
         } else if (this.target.isTraceEnabled()) {
            this.target
               .trace()
               .append(this.classname)
               .append(".checkDelayedBorrowState - Dont borrow because there is another own audio connection active")
               .log();
         }

         this.properties.setDelayBorrowAudio(false);
      }
   }

   public void handleTaActive() {
      if (this.speechapplicant.isCarplayActive()) {
         if (this.target.isTraceEnabled()) {
            this.target.trace().append(this.classname).append(".handleTaActive - cancel running siri session to make the TA hearable").log();
         }

         this.speechapplicant.forceChangeState(ActivationState.INACTIVE);
         this.target.getDSICarPlay().requestSIRIAction(1);
         this.target.getDSICarPlay().requestSIRIAction(2);
      }
   }

   public void checkForSync() {
      if (this.target.isTraceEnabled()) {
         this.target.trace().append(this.classname).append(".checkForSync - check video source sync...").log();
      }

      if (this.properties.getCurrentScreenSource() == 2) {
         if (!this.properties.isCanvasVisible()) {
            if (this.target.isTraceEnabled()) {
               this.target
                  .trace()
                  .append(this.classname)
                  .append(".checkForSync - async video source found: VideoMode = Device but canvas invisible => TAKE video source to get sync...")
                  .log();
            }

            this.modehandling.takeScreen("Take Screen: Watchdog found an async video source - canvas is invisible");
         } else if (this.properties.isCanvasVisible() && this.properties.isCanvasDimed()) {
            if (this.target.isTraceEnabled()) {
               this.target
                  .trace()
                  .append(this.classname)
                  .append(
                     ".checkForSync - async video source found: VideoMode = Device, canvas is visible BUT popup is open => BORROW video source to get sync..."
                  )
                  .log();
            }

            this.modehandling.borrowScreen("Borrow Screen: Watchdog found an async video source - canvas is visible but a popup is open");
         } else if (this.properties.isCanvasVisible() && !this.properties.isCanvasDimed() && this.target.isTraceEnabled()) {
            this.target.trace().append(this.classname).append(".checkForSync - video source is in sync.").log();
         }
      } else if (this.properties.getCurrentScreenSource() == 1) {
         if (this.properties.isCanvasVisible() && !this.properties.isCanvasDimed()) {
            if (this.properties.isCanvasVisible() && !this.properties.isCanvasDimed()) {
               if (this.target.isTraceEnabled()) {
                  this.target
                     .trace()
                     .append(this.classname)
                     .append(".checkForSync - async video source found: VideoMode = HMI but canvas visible and no popup open => REQUEST_UI to get sync...")
                     .log();
               }

               this.target.getDSICarPlay().requestUI(0);
            }
         } else if (this.target.isTraceEnabled()) {
            this.target.trace().append(this.classname).append(".checkForSync -  video source is in sync.").log();
         }
      } else if (this.properties.getCurrentScreenSource() == 0) {
         if (this.target.isTraceEnabled()) {
            this.target
               .trace()
               .append(this.classname)
               .append(".checkForSync - video source is set to unknown => tell the device the state we are currently in...")
               .log();
         }

         if (!this.properties.isCanvasVisible()) {
            if (this.target.isTraceEnabled()) {
               this.target.trace().append(this.classname).append(".checkForSync - canvas is invisible => TAKE video source...").log();
            }

            this.modehandling.takeScreen("Take Screen: Watchdog found an async video source - canvas is invisible");
         } else if (this.properties.isCanvasVisible() && this.properties.isCanvasDimed()) {
            if (this.target.isTraceEnabled()) {
               this.target.trace().append(this.classname).append(".checkForSync - canvas is visible but with popup => BORROW video source...").log();
            }

            this.modehandling.borrowScreen("Borrow Screen: Watchdog found an async video source - canvas is visible but a popup is open");
         } else if (this.properties.isCanvasVisible() && !this.properties.isCanvasDimed()) {
            if (this.target.isTraceEnabled()) {
               this.target.trace().append(this.classname).append(".checkForSync - canvas is visible without a popup => REQUEST_UI...").log();
            }

            this.target.getDSICarPlay().requestUI(0);
         }
      }

      if (this.target.isTraceEnabled()) {
         this.target.trace().append(this.classname).append(".checkForSync - check audio source sync...").log();
      }

      if (this.properties.getCurrentMainAudioSource() == 2) {
         if (this.properties.isSoundManagementAvailable()) {
            switch (this.properties.getCurrentMainAudioType()) {
               case 0:
                  if (this.target.isTraceEnabled()) {
                     this.target.trace().append(this.classname).append(".checkForSync - Source = Device, Type = NONE => Nothing to check.").log();
                  }
                  break;
               case 1:
                  if (this.target.isTraceEnabled()) {
                     this.target.trace().append(this.classname).append(".checkForSync - Source = Device, Type = Alert => Check ringtone...").log();
                  }

                  if (!this.audiohandling.checkAudioSourceState(this.properties.getRingtoneConnection())) {
                     if (this.target.isTraceEnabled()) {
                        this.target
                           .trace()
                           .append(this.classname)
                           .append(".checkForSync - async audio source found - ringtone is not set => TAKE audio...")
                           .log();
                     }

                     this.audiohandling.takeMainAudio("Take Audio: Watchdog found an async audio source - ringtone connection is not set");
                  }
                  break;
               case 2:
                  if (this.target.isTraceEnabled()) {
                     this.target
                        .trace()
                        .append(this.classname)
                        .append(".checkForSync - Source = Device, Type = Media => Check media connection and entertainment manager...")
                        .log();
                  }

                  if (!this.audiohandling.checkAudioSourceState(162)) {
                     if (this.target.isTraceEnabled()) {
                        this.target.trace().append(this.classname).append(".checkForSync - async audio source found - media is not set => TAKE audio...").log();
                     }

                     this.audiohandling.takeMainAudio("Take Audio: Watchdog found an async audio source - media connection is not set");
                  } else if (!this.properties.isActiveEntertainment()) {
                     if (this.target.isTraceEnabled()) {
                        this.target
                           .trace()
                           .append(this.classname)
                           .append(".checkForSync - async audio source found - we are not entertainment source => TAKE audio...")
                           .log();
                     }

                     this.audiohandling.takeMainAudio("Take Audio: Watchdog found an async audio source - carplay is not the active entertainment source");
                  }
                  break;
               case 3:
                  if (this.target.isTraceEnabled()) {
                     this.target.trace().append(this.classname).append(".checkForSync - Source = Device, Type = Phone => Check phone and phone input...").log();
                  }

                  if (!this.audiohandling.checkAudioSourceState(151) || !this.audiohandling.checkAudioSourceState(163)) {
                     if (this.target.isTraceEnabled()) {
                        this.target
                           .trace()
                           .append(this.classname)
                           .append(".checkForSync - async audio source found - input or output is not set => TAKE audio...")
                           .log();
                     }

                     this.audiohandling.takeMainAudio("Take Audio: Watchdog found an async audio source - phone connection is not set (in or out)");
                  }
                  break;
               case 4:
                  if (this.target.isTraceEnabled()) {
                     this.target
                        .trace()
                        .append(this.classname)
                        .append(".checkForSync - Source = Device, Type = Speech => Check speech and speech input...")
                        .log();
                  }

                  if (!this.audiohandling.checkAudioSourceState(152) || !this.audiohandling.checkAudioSourceState(153)) {
                     if (this.target.isTraceEnabled()) {
                        this.target
                           .trace()
                           .append(this.classname)
                           .append(".checkForSync - async audio source found - input or output is not set => TAKE audio...")
                           .log();
                     }

                     this.audiohandling.takeMainAudio("Take Audio: Watchdog found an async audio source - speech connection is not set (in or out)");
                  }
            }
         } else {
            if (this.target.isTraceEnabled()) {
               this.target
                  .trace()
                  .append(this.classname)
                  .append(".checkForSync - audio source = device but sound management not available => BORROW audio...")
                  .log();
            }

            this.audiohandling.borrowMainAudio("Borrow audio: audio management is not available (triggered by watchdog)");
         }
      } else if (this.properties.getCurrentMainAudioSource() == 1) {
         if (this.target.isTraceEnabled()) {
            this.target.trace().append(this.classname).append(".checkForSync - Source = MainUnit => check if all audio connections are released...").log();
         }

         if (this.audiohandling.checkAudioSourceState(this.properties.getRingtoneConnection())
            || this.audiohandling.checkAudioSourceState(151)
            || this.audiohandling.checkAudioSourceState(163)
            || this.audiohandling.checkAudioSourceState(152)
            || this.audiohandling.checkAudioSourceState(153)) {
            if (this.target.isTraceEnabled()) {
               this.target
                  .trace()
                  .append(this.classname)
                  .append(".checkForSync - async audio found - one or more audio connections are still present but not needed => release all of them...")
                  .log();
            }

            this.audiohandling.releaseAllConnections(false, false);
         }
      }

      if (this.target.isTraceEnabled()) {
         this.target.trace().append(this.classname).append(".checkForSync - all sync tests done.").log();
      }
   }

   public void executeTeardown() {
      if (this.properties.isConnected() && this.properties.getCurrentMainAudioSource() != 1) {
         this.audiohandling.handleAudioSourceChange(0, this.properties.getCurrentMainAudioSource(), true);
         this.properties.setCurrentMainAudioType(0);
      } else if (this.target.isTraceEnabled()) {
         this.target.trace().append(this.classname).append(".executeTeardown - teardown no longer needed").log();
      }
   }

   public void executeEdgeDelay() {
      if (this.properties.isConnected()) {
         if (this.properties.isDoEdgeDelayExecution()) {
            if (!this.properties.isCanvasVisible()) {
               if (this.target.isTraceEnabled()) {
                  this.target.trace("CARPLAY - trigger ASL_SMARTPHONEINTEGRATION_CARPLAY_TRIGGERED_CONTEXTSWITCH_TO_CARPLAY");
               }

               this.target.triggerObserver(6100008, null);
               this.properties.setVideoBorrowState(false);
            }

            if (this.properties.isCanvasDimed()) {
               if (this.target.isTraceEnabled()) {
                  this.target.trace("CARPLAY - screen requested but a popup is still open -> borrow screen back");
               }

               this.modehandling.borrowScreen("Borrow Screen: Screen was requested but a popup is open (during execution of edge delay)");
            }
         } else if (this.target.isTraceEnabled()) {
            this.target.trace().append(this.classname).append(".executeEdgeDelay - execution not requested - switch display source not needed").log();
         }
      } else if (this.target.isTraceEnabled()) {
         this.target.trace().append(this.classname).append(".executeEdgeDelay - disconnected - switch display source no longer needed").log();
      }

      this.properties.setDoEdgeDelayExecution(false);
   }

   public void handleAslApiOcuCallState(int var1) {
      if (this.properties.isConnected()) {
         if (var1 != 0) {
            CarPlayHMIRequestParameterConfiguration var2 = this.properties.getParamconfig();
            super.getClass();
            ResourceRequest[] var3 = var2.getRessourceRequest(0);
            CarPlayHMIRequestParameterConfiguration var4 = this.properties.getParamconfig();
            super.getClass();
            this.target.getDSICarPlay().requestModeChange(var3, var4.getAppStateRequest(1), "OCU call has started");
         } else if (this.properties.getOcuCallState() != 0) {
            CarPlayHMIRequestParameterConfiguration var5 = this.properties.getParamconfig();
            super.getClass();
            ResourceRequest[] var6 = var5.getRessourceRequest(0);
            CarPlayHMIRequestParameterConfiguration var7 = this.properties.getParamconfig();
            super.getClass();
            this.target.getDSICarPlay().requestModeChange(var6, var7.getAppStateRequest(7), "OCU call has stopped");
         }
      }

      this.properties.setOcuCallState(var1);
   }
}
