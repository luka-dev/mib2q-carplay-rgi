package de.vw.mib.asl.internal.exboxm.api.impl.guidance;

import de.vw.mib.asl.api.exboxm.guidance.ExboxGuidanceListener;
import de.vw.mib.asl.api.exboxm.guidance.ExboxGuidanceManager;
import de.vw.mib.asl.api.exboxm.guidance.GuidanceState;
import de.vw.mib.asl.internal.exboxm.api.impl.guidance.ExboxGuidanceManagerImpl.1;
import de.vw.mib.asl.internal.exboxm.state.ExboxStateService;
import de.vw.mib.asl.internal.exboxm.util.ExboxLogger;
import de.vw.mib.asl.internal.exboxm.util.Preconditions;
import de.vw.mib.genericevents.ThreadSwitchingTarget;
import de.vw.mib.log4mib.Logger;
import java.util.HashSet;
import java.util.Set;

public class ExboxGuidanceManagerImpl implements ExboxGuidanceManager, ExboxGuidanceListener {
   private ExboxStateService exboxStateService;
   private ExboxLogger exboxLogger;
   private ThreadSwitchingTarget threadSwitchingTarget;
   private Set listeners;

   public ExboxGuidanceManagerImpl(Logger var1, ExboxStateService var2, ThreadSwitchingTarget var3) {
      Preconditions.checkArgumentNotNull(var1, "EX: Logger must not be null!");
      Preconditions.checkArgumentNotNull(var2, "EX: ExboxStateService must not be null!");
      Preconditions.checkArgumentNotNull(var3, "EX: ThreadSwitchingTarget must not be null!");
      this.exboxLogger = new ExboxLogger(var1, super.getClass());
      this.exboxStateService = var2;
      this.exboxStateService.setGuidanceListener(this);
      this.threadSwitchingTarget = var3;
      this.listeners = new HashSet();
   }

   public void addListener(ExboxGuidanceListener var1) {
      if (var1 == null) {
         throw new IllegalArgumentException("EX: Incoming ExboxGuidanceListener instance is null!");
      }

      if (this.listeners.contains(var1)) {
         throw new IllegalArgumentException("EX: Incoming ExboxGuidanceListener instance is already registered!");
      }

      this.listeners.add(var1);
   }

   public void changeSmartphoneGuidanceStateTo(GuidanceState var1) {
      System.out.println("changeSmartphoneGuidanceStateTo(" + var1 + ")");
      new Exception("Stack trace").printStackTrace();
      this.exboxStateService.setSmartphoneGuidanceState(var1);
   }

   public GuidanceState getGuidanceState() {
      GuidanceState var1 = this.exboxStateService.getExboxGuidanceState();
      System.out.println("getGuidanceState():" + var1);
      new Exception("Stack trace").printStackTrace();
      return var1;
   }

   public void onExboxGuidanceStateChanged(GuidanceState var1) {
      System.out.println("onExboxGuidanceStateChanged(" + var1 + ")");
      new Exception("Stack trace").printStackTrace();
      this.notifyListeners(var1);
   }

   public void removeListener(ExboxGuidanceListener var1) {
      this.listeners.remove(var1);
   }

   private void notifyListener(ExboxGuidanceListener var1, GuidanceState var2) {
      this.threadSwitchingTarget.enqueue(new 1(this, var1, var2));
   }

   private void notifyListeners(GuidanceState var1) {
      if (this.listeners.isEmpty()) {
         this.exboxLogger.warn("No instances of ExboxGuidanceListener registered.");
      } else {
         for (ExboxGuidanceListener var3 : this.listeners) {
            this.notifyListener(var3, var1);
         }
      }
   }
}
