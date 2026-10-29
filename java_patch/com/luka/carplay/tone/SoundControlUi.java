/*
 * Native Amplifier Unlock on Sound (tone screen 1000001), Sound -> Sound effects
 * (tone screen 1000006), and VNC/GALA rotary (tone screen 1000007):
 *   1. Exposes the stock TONE_SURROUND_ON_OFF_CHOICE (1000116) model as a top-of-menu
 *      "Surround" checkbox (wired by stock de.audi.tone.app.sound.SurroundRange to
 *      DSISound.setSurroundOnOff -> MOST FktID 0x0D28 PosX=3, toggling Row 0 Linear
 *      passthrough on Alpine B&O 0x140, Centerpoint on Bose 0x160/0x161, and Symphoria
 *      on B&O Advanced 0x181), with Infoline hint 1001007 from lang_data.zip.
 *   2. Unlocks the stock SUBWOOFER_RANGE (1000015, -9..+9) control on amplifiers with
 *      a physical subwoofer channel (Bose 3D 0x160, B&O Advanced 0x181, etc.) while
 *      keeping it hidden on 2ch/4ch internal amps without a subwoofer.
 *   3. Matches the stock VNC / GALA / AudioPilot widget (AMPLIFIER_VARIANT_CHOICE 1000035)
 *      and range (DSP_LEVELLING_RANGE 1000039 / AUDIOPILOT_CHOICE 1000046) to each
 *      amplifier's actual hardware:
 *        - Bose (0x160/0x161/Porsche=17/18/19): stock Checkbox "AudioPilot" (0..1)
 *        - Internal DSP (0x120..0x122=1..3): stock Rotary (0..7)
 *        - Alpine B&O 16ch / Bentley (0x140/0x141/0x151=4/5): stock Rotary (0..7)
 *        - B&O 3D Adv / Bentley Adv / Burmester (0x181/0x182=21/22/23): stock Rotary (0..24)
 *
 * CarPlay-specific audio settings (Spatial Audio + UpMix) live in Connection
 * manager -> Options -> CarPlay settings (CarPlaySettingsUi).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.tone;

import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.model.ChoiceModel;
import de.audi.atip.hmi.model.RangeModel;
import de.audi.atip.hmi.view.HMIView;
import de.audi.atip.hmi.view.Screen;
import de.audi.atip.model.ICoreToneModelBank;
import de.audi.atip.model.ToneModelBank;
import de.audi.tghu.tone.hmi.evohigh.ToneScreenFactory;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import de.esolutions.hmi.widgets.audi.base.AbstractWidget;
import de.esolutions.hmi.widgets.audi.base.StringUtility;
import de.esolutions.hmi.widgets.audi.evo.high.widgets.AbstractRendererHigh;
import de.esolutions.hmi.widgets.audi.evo.high.widgets.CompositeRendererHigh;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.InfolineController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.InfolineTimerController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuItemController;

public final class SoundControlUi {
    public static final int MODEL_SURROUND = ICoreToneModelBank.TONE_SURROUND_ON_OFF_CHOICE;        // 1000116
    private static final int INDEX_SURROUND = MODEL_SURROUND % 100000;
    public static final int MODEL_SUBWOOFER = ICoreToneModelBank.SUBWOOFER_RANGE;                   // 1000015
    private static final int INDEX_SUBWOOFER = MODEL_SUBWOOFER % 100000;
    public static final int MODEL_AMPLIFIER_VARIANT = ICoreToneModelBank.AMPLIFIER_VARIANT_CHOICE;  // 1000035
    private static final int INDEX_AMPLIFIER_VARIANT = MODEL_AMPLIFIER_VARIANT % 100000;
    public static final int MODEL_VNC = ICoreToneModelBank.DSP_LEVELLING_RANGE;                     // 1000039
    private static final int INDEX_VNC = MODEL_VNC % 100000;
    public static final int MODEL_AUDIOPILOT = ICoreToneModelBank.AUDIOPILOT_CHOICE;                // 1000046
    private static final int INDEX_AUDIOPILOT = MODEL_AUDIOPILOT % 100000;

    private static final int SCREEN_SOUND_EFFECTS = 1000006;
    public static final int TEXT_SURROUND_LABEL = 1001006; // tone table slot 1006 in lang_data.zip
    public static final int TEXT_SURROUND_HINT  = 1001007; // tone table slot 1007 in lang_data.zip
    private static final int INTERNAL_ID = 1009910;        // focus-restore key, unique in the menu

    private static volatile int amplifier = -1;
    private static volatile ChoiceModel surroundModel;
    private static volatile ChoiceModel amplifierVariantModel;
    private static volatile ChoiceModel audioPilotModel;
    private static volatile int lastRawAmplifierVariant;
    private static volatile RangeModel subwooferModel;
    private static volatile RangeModel vncModel;
    private static volatile boolean vncValueReceived;
    private static volatile MenuItemController surroundItem;
    private static volatile MenuItemController surroundLevelItem;

    private SoundControlUi() {}

    public static ToneModelBank newBank() { return new Bank(); }

    public static ToneScreenFactory newFactory(IFrameworkAccess fw) { return new Factory(fw); }

    /** True on external surround-capable amplifiers (B&O 0x140=4, Bose 0x160/0x161=17/18, B&O Adv 0x181=21, etc.). */
    public static boolean surroundSupported(int type) {
        return type != 1 && type != 2 && type != 3;
    }

    public static boolean surroundSupported() {
        return surroundSupported(amplifier);
    }

    /** True on amplifiers with a dedicated subwoofer channel (9VD 6ch=3, Bose=17/18, B&O=4/21, etc.; false on 2ch=1 / 4ch=2). */
    public static boolean subwooferSupported(int type) {
        return type != 1 && type != 2;
    }

    public static boolean subwooferSupported() {
        return subwooferSupported(amplifier);
    }

    /** True on Bose amplifiers (0x160=17, 0x161=18, Porsche Bose=19) whose hardware AudioPilot is a boolean 0..1 checkbox. */
    public static boolean boseAudioPilotAmp(int type) {
        return type == 17 || type == 18 || type == 19;
    }

    /** Max rotary level for VNC/GALA (DSP_LEVELLING_RANGE 1000039): 0 on Bose checkbox amps, 24 on B&O Adv / Burmester, 7 on Internal / Alpine B&O. */
    public static int vncMaxLevel(int type) {
        if (boseAudioPilotAmp(type)) return 0;
        if (type == 21 || type == 22 || type == 23) return 24;
        return 7;
    }

    public static int vncMaxLevel() {
        return vncMaxLevel(amplifier);
    }

    /**
     * Factory default VNC / GALA / AudioPilot level from MIB_AudioTable_AU_G22.xml and Bose MAIN.bin:
     *   - Bose (17, 18, 19): 1 (AudioPilot On)
     *   - B&O 3D Adv / Bentley Adv / Burmester (21, 22, 23): 12 (50% of 0..24)
     *   - Internal DSP (1..3) and Alpine B&O / Bentley (4, 5): 2 (from 0..7)
     */
    public static int vncDefaultLevel(int type) {
        if (boseAudioPilotAmp(type)) return 1;
        if (type == 21 || type == 22 || type == 23) return 12;
        return 2;
    }

    public static int vncDefaultLevel() {
        return vncDefaultLevel(amplifier);
    }

    /**
     * Map SOUND_SYSTEM_CHOICE (1000035) so ToneScreenFactory selects the stock widget
     * matching the amplifier's actual VNC / GALA / AudioPilot hardware:
     *   - 1..3: Internal C674x DSP (0x120..0x122) -> menuitemcontroller6 Rotary (0..7)
     *   - 4:    Bose (0x160=17, 0x161=18, Porsche=19) -> menuitemcontroller7 Checkbox (0..1 "AudioPilot")
     *   - 5:    Alpine B&O 16ch / Bentley (0x140=4, 0x141/0x151=5) -> menuitemcontroller8 Rotary (0..7)
     *   - 6:    B&O 3D Adv / Bentley Adv / Burmester (0x181=21, 0x182=22, 23) -> menuitemcontroller9 Rotary (0..24)
     */
    public static int soundSystemChoice(int type, int rawChoice) {
        if (rawChoice <= 0) return rawChoice;
        if (type == 1 || type == 2 || type == 3) return type;
        if (boseAudioPilotAmp(type) || rawChoice == 4 || rawChoice == 8) return 4;
        if (type == 4 || type == 5 || rawChoice == 5 || rawChoice == 9) return 5;
        if (type == 21 || type == 22 || type == 23 || rawChoice == 6 || rawChoice == 7 || rawChoice == 10) return 6;
        return rawChoice;
    }

    public static int soundSystemChoice(int rawChoice) {
        return soundSystemChoice(amplifier, rawChoice);
    }

    /** Re-evaluate Amplifier Variant, Subwoofer, and VNC range limits when the active amplifier type is reported. */
    public static void updateAmplifier(int type) {
        amplifier = type;
        try {
            ChoiceModel ampVar = amplifierVariantModel;
            if (ampVar != null && lastRawAmplifierVariant > 0) {
                ampVar.setValue(lastRawAmplifierVariant);
            }
            RangeModel sub = subwooferModel;
            if (sub != null) {
                if (subwooferSupported(type)) {
                    if (sub.getMinimum() == 0 && sub.getMaximum() == 0) {
                        sub.setLimits(-9, 9, 1, 0);
                    }
                } else {
                    sub.setLimits(0, 0, 1, 0);
                }
            }
            RangeModel vnc = vncModel;
            if (vnc != null) {
                int vncMax = vncMaxLevel(type);
                vnc.setLimits(0, vncMax, 1, vncMax / 2);
                if (!vncValueReceived && vncMax > 0) {
                    vnc.setValue(vncDefaultLevel(type));
                }
            }
            ChoiceModel ap = audioPilotModel;
            if (ap != null && !vncValueReceived) {
                ap.setValue(1);
            }
        } catch (Throwable t) { /* ignore */ }
    }

    static void syncSurroundLevelEnabled(int terminal) {
        try {
            boolean stockDisabled = Factory.isSurroundLevelDisabledByStock(terminal);
            MenuItemController sItem = surroundItem;
            if (sItem != null) {
                sItem.setEnabled(!stockDisabled);
            }
            MenuItemController slItem = surroundLevelItem;
            if (slItem != null) {
                ChoiceModel m = surroundModel;
                boolean surroundOn = (m == null || m.getValue() != 0);
                slItem.setEnabled(surroundOn && !stockDisabled);
            }
        } catch (Throwable t) { /* ignore */ }
    }

    static final class Bank extends ToneModelBank {
        protected synchronized void createModel(int i) {
            if (i == INDEX_SURROUND) {
                if (this.models[i] != null) return;
                ChoiceModel m = new ChoiceModel(MODEL_SURROUND) {
                    public void setValue(int value) {
                        super.setValue(value);
                        syncSurroundLevelEnabled(0);
                    }
                    public void itemSelected(int value, int terminal) {
                        this.setValue(value);
                        super.itemSelected(value, terminal);
                    }
                };
                m.setValue(1);
                this.models[i] = m;
                surroundModel = m;
                return;
            }
            if (i == INDEX_SUBWOOFER) {
                if (this.models[i] != null) return;
                RangeModel m = new RangeModel(MODEL_SUBWOOFER) {
                    public void setLimits(int min, int max, int step) {
                        if (min == 0 && max == 0 && subwooferSupported()) {
                            super.setMedialPosition(0);
                            super.setLimits(-9, 9, step > 0 ? step : 1);
                        } else {
                            super.setLimits(min, max, step);
                        }
                    }
                };
                if (subwooferSupported()) {
                    m.setLimits(-9, 9, 1, 0);
                }
                this.models[i] = m;
                subwooferModel = m;
                return;
            }
            if (i == INDEX_AMPLIFIER_VARIANT) {
                if (this.models[i] != null) return;
                ChoiceModel m = new ChoiceModel(MODEL_AMPLIFIER_VARIANT) {
                    public void setValue(int value) {
                        lastRawAmplifierVariant = value;
                        super.setValue(soundSystemChoice(value));
                    }
                };
                this.models[i] = m;
                amplifierVariantModel = m;
                return;
            }
            if (i == INDEX_VNC) {
                if (this.models[i] != null) return;
                final boolean[] initDone = new boolean[]{false};
                RangeModel m = new RangeModel(MODEL_VNC) {
                    public void setLimits(int min, int max, int step) {
                        int vncMax = vncMaxLevel();
                        if (vncMax > 0) {
                            super.setMedialPosition(vncMax / 2);
                            super.setLimits(0, vncMax, step > 0 ? step : 1);
                        } else {
                            super.setMedialPosition(0);
                            super.setLimits(0, 0, step > 0 ? step : 1);
                        }
                    }
                    public void setValue(int value) {
                        if (initDone[0]) vncValueReceived = true;
                        super.setValue(value);
                    }
                    public void increment(int steps, int terminal) {
                        int max = getMaximum();
                        int cur = getValue();
                        if (cur >= max) return;
                        if (cur + steps > max) steps = max - cur;
                        super.increment(steps, terminal);
                    }
                    public void decrement(int steps, int terminal) {
                        int min = getMinimum();
                        int cur = getValue();
                        if (cur <= min) return;
                        if (cur - steps < min) steps = cur - min;
                        super.decrement(steps, terminal);
                    }
                };
                int vncMax = vncMaxLevel();
                if (vncMax > 0) {
                    m.setLimits(0, vncMax, 1, vncMax / 2);
                    m.setValue(vncDefaultLevel());
                }
                initDone[0] = true;
                this.models[i] = m;
                vncModel = m;
                return;
            }
            if (i == INDEX_AUDIOPILOT) {
                if (this.models[i] != null) return;
                final boolean[] initDone = new boolean[]{false};
                ChoiceModel m = new ChoiceModel(MODEL_AUDIOPILOT) {
                    public void setValue(int value) {
                        if (initDone[0]) vncValueReceived = true;
                        super.setValue(value > 0 ? 1 : 0);
                    }
                };
                m.setValue(1);
                initDone[0] = true;
                this.models[i] = m;
                audioPilotModel = m;
                return;
            }
            super.createModel(i);
        }
    }

    static final class Factory extends ToneScreenFactory {
        Factory(IFrameworkAccess fw) { super(fw); }

        static boolean isSurroundLevelDisabledByStock(int terminal) {
            try {
                return evalCond1000106(terminal);
            } catch (Throwable t) {
                return false;
            }
        }

        protected Screen createScreen(int id, int terminal) {
            Screen s = super.createScreen(id, terminal);
            if (id == SCREEN_SOUND_EFFECTS && surroundSupported()) {
                try { addSurroundRow(s, terminal); } catch (Throwable t) { /* stock screen unchanged on error */ }
            }
            return s;
        }

        public void executeCondition(int screenId, int modelId, HMIView[] views, int terminal) {
            super.executeCondition(screenId, modelId, views, terminal);
            if (screenId == SCREEN_SOUND_EFFECTS) {
                syncSurroundLevelEnabled(terminal);
            }
        }

        public String getText(int id) {
            String s = super.getText(id);
            if (s == null || s.length() == 0) {
                if (id == TEXT_SURROUND_LABEL) return "Surround";
                if (id == TEXT_SURROUND_HINT) return "Turn off to bypass surround processing for direct linear audio";
            }
            return s;
        }

        public int getTextStatus(int id) {
            int st = super.getTextStatus(id);
            if (st == 0 && (id == TEXT_SURROUND_LABEL || id == TEXT_SURROUND_HINT)) {
                return 8;
            }
            return st;
        }

        private static void addSurroundRow(Screen s, int terminal) {
            AbstractScreenWidget screen = (AbstractScreenWidget) s;
            HMIView[] refViews = screen.findViews(ICoreToneModelBank.THREE_D_MODE_CHOICE);
            MenuItemController ref = (MenuItemController) refViews[0];
            AbstractWidget parent = ref.getParent();
            if (!(parent instanceof MenuController)) return;
            MenuController menu = (MenuController) parent;

            HMIView[] surrViews = screen.findViews(ICoreToneModelBank.DSP_SOURROUND_RANGE);
            if (surrViews != null && surrViews.length > 0 && surrViews[0] instanceof MenuItemController) {
                surroundLevelItem = (MenuItemController) surrViews[0];
            }

            InfolineTimerController timer = menu.getInfolineTimer();
            if (timer != null) {
                timer.setStartTimerAutomatically(true);
            }
            InfolineController infoline = menu.getInfoline();
            if (infoline != null && infoline.getInfolineRenderer() != null) {
                infoline.getInfolineRenderer().setAutoWrap(StringUtility.WRAP_MODE_STANDARD);
            }

            MenuItemController item = new MenuItemController();
            CompositeRendererHigh r = new CompositeRendererHigh(item);
            item.setRenderer(r);
            r.setFonts(((AbstractRendererHigh) ref.getRenderer()).getFonts());
            item.setModelID(MODEL_SURROUND);
            item.setMapping(new int[][]{{0}, {1}});
            item.setLabelId(TEXT_SURROUND_LABEL);
            item.setInfolineTextIdEnabled(TEXT_SURROUND_HINT);
            item.setHasInfolineDisclaimer(false);
            item.setGlassplateInsetsBottom(6);
            item.setGlassplateInsetsTop(7);
            item.setInternalID(INTERNAL_ID);
            item.setType(MenuItemController.TYPE_CHECKBOX);
            item.setWidgetID(1);

            // Views first: if this throws, the menu is untouched.
            int[] ids = screen.getViewIDs();
            HMIView[][] views = new HMIView[ids.length + 1][];
            int[] nids = new int[ids.length + 1];
            int o = 0;
            boolean placed = false;
            for (int k = 0; k < ids.length; k++) {      // findViews() binary-searches: keep ids sorted
                if (!placed && ids[k] > MODEL_SURROUND) {
                    nids[o] = MODEL_SURROUND;
                    views[o++] = new HMIView[]{item};
                    placed = true;
                }
                nids[o] = ids[k];
                views[o++] = screen.findViews(ids[k]);
            }
            if (!placed) {
                nids[o] = MODEL_SURROUND;
                views[o] = new HMIView[]{item};
            }
            screen.setViews(nids, views);

            int pos = 1;
            if (surroundLevelItem != null) {
                int idx = menu.getChildren().indexOf(surroundLevelItem);
                if (idx >= 0) pos = idx;
            }
            menu.addWithPosition(item, pos);
            surroundItem = item;
            syncSurroundLevelEnabled(terminal);
        }
    }
}
