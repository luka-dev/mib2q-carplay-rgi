/*
 * "UpMix" row on Sound -> Sound effects (tone screen 1000006).
 *
 * Model: ChoiceModel 1000162 = tone module 10, index 162 (stock bank has 0..161). It lives in the
 * stock ToneModelBank instance (subclass below), so HMIRegistry/ModelConnectService resolve it like
 * THREE_D_MODE_CHOICE. Texts: private ids 1009900..1009905 answered by the factory's getText
 * from the tone string table, slots 9900..9905 the installer fills (lang_data.zip), else from
 * the built-in English/Spanish/French table.  Wired by the replaced tone Activator; every failure falls back to the stock
 * bank/factory or leaves the stock screen (docs/audio/STEREO_TO_5_1_UPMIX.md, "MMI level").
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.tone;

import com.luka.carplay.core.UpmixSetting;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.model.ChoiceModel;
import de.audi.atip.hmi.model.HMIModel;
import de.audi.atip.hmi.model.listener.DefaultChoiceListener;
import de.audi.atip.hmi.view.HMIView;
import de.audi.atip.hmi.view.Screen;
import de.audi.atip.i18n.I18NTarget;
import de.audi.atip.model.ICoreToneModelBank;
import de.audi.atip.model.ToneModelBank;
import de.audi.tghu.tone.hmi.evohigh.ToneScreenFactory;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import de.esolutions.hmi.widgets.audi.base.AbstractWidget;
import de.esolutions.hmi.widgets.audi.evo.high.widgets.AbstractRendererHigh;
import de.esolutions.hmi.widgets.audi.evo.high.widgets.CompositeRendererHigh;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuItemController;

public final class UpmixToneUi {
    public static final int MODEL_ID = 1000162;           // module 10, index 162
    private static final int INDEX = MODEL_ID % 100000;
    private static final int SCREEN_SOUND_EFFECTS = 1000006;
    private static final int TEXT_LABEL = 1009900;         // tone table slot 9900; + 1..5 = options
    private static final int INTERNAL_ID = 1009910;        // focus-restore key, unique in the menu
    /* label + the five options per HMI language of the tone string tables (en_US, es_MX, fr_CA);
     * "Off" is the stock wording in all three (tone text 1000303). Any other language gets English. */
    private static final String[] TEXTS_EN = {"UpMix", "Off", "Moderate", "Normal", "Full", "Aggressive"};
    private static final String[] TEXTS_ES = {"UpMix", "Off", "Moderado", "Normal", "Completo", "Agresivo"};
    private static final String[] TEXTS_FR = {"UpMix", "Off", "Mod\u00e9r\u00e9", "Normal", "Complet", "Agressif"};

    private static volatile ChoiceModel model;
    private static volatile int pending = -1;              // show() before the model exists

    private UpmixToneUi() {}

    public static ToneModelBank newBank() { return new Bank(); }

    public static ToneScreenFactory newFactory(IFrameworkAccess fw) { return new Factory(fw); }

    /** Reflect the active level (0..4) in the row. Any thread; never throws. */
    public static void show(int level) {
        try {
            ChoiceModel m = model;
            if (m == null) { pending = level; return; }
            m.setValue(level);
        } catch (Throwable t) { /* ponytail: UI mirror only */ }
    }

    static final class Bank extends ToneModelBank {
        Bank() {
            HMIModel[] grown = new HMIModel[INDEX + 1];
            System.arraycopy(this.models, 0, grown, 0, this.models.length);
            this.models = grown;
            int[] ids = new int[this.modelIDs.length + 1];
            System.arraycopy(this.modelIDs, 0, ids, 0, this.modelIDs.length);
            ids[ids.length - 1] = MODEL_ID;
            this.modelIDs = ids;
        }

        protected synchronized void createModel(int i) {
            if (i != INDEX) { super.createModel(i); return; }
            if (this.models[i] != null) return;
            ChoiceModel m = new ChoiceModel(MODEL_ID);
            m.setChoiceListener(new DefaultChoiceListener() {
                public void itemSelected(int id, int value, int unused, int terminal) {
                    try {
                        model.setValue(value);      // combo already shows it; keep model in sync
                        UpmixSetting.select(value);
                    } catch (Throwable t) { /* never into the HMI thread */ }
                }
            });
            if (pending >= 0) m.setValue(pending);
            this.models[i] = m;
            model = m;
            try { UpmixSetting.attach(); } catch (Throwable t) { /* row stays, no hook mirror */ }
        }
    }

    static final class Factory extends ToneScreenFactory {
        Factory(IFrameworkAccess fw) { super(fw); }

        protected Screen createScreen(int id, int terminal) {
            Screen s = super.createScreen(id, terminal);
            if (id == SCREEN_SOUND_EFFECTS) {
                try { addRow(s); } catch (Throwable t) { /* stock screen unchanged up to the failure point */ }
            }
            return s;
        }

        public String getText(int id) {
            int k = id - TEXT_LABEL;
            return k >= 0 && k < TEXTS_EN.length ? text(k, super.getText(id)) : super.getText(id);
        }

        public String getText(int id, int viewSize) {
            int k = id - TEXT_LABEL;
            return k >= 0 && k < TEXTS_EN.length ? text(k, super.getText(id, viewSize)) : super.getText(id, viewSize);
        }

        /* The installer puts the six texts into the tone string table at slots 9900..9905
         * (deploy/lang/upmix_lang.py), so the stock lookup answers our ids directly; an empty
         * slot (no micropython at install, a skipped locale) falls back to the built-in table. */
        private String text(int k, String stock) {
            if (stock != null && stock.length() != 0) return stock;
            try {
                String code = getFramework().getLanguageMgr()
                        .getCurrentLanguage(I18NTarget.LANG_COMPONENT_HMI).getHmiCode();
                if (code != null && code.startsWith("es")) return TEXTS_ES[k];
                if (code != null && code.startsWith("fr")) return TEXTS_FR[k];
            } catch (Throwable t) { /* English below */ }
            return TEXTS_EN[k];
        }

        private static void addRow(Screen s) {
            AbstractScreenWidget screen = (AbstractScreenWidget) s;
            HMIView[] refViews = screen.findViews(ICoreToneModelBank.THREE_D_MODE_CHOICE);
            MenuItemController ref = (MenuItemController) refViews[0];
            AbstractWidget parent = ref.getParent();
            if (!(parent instanceof MenuController)) return;

            MenuItemController item = new MenuItemController();
            CompositeRendererHigh r = new CompositeRendererHigh(item);
            item.setRenderer(r);
            item.setBitmaps(ref.getBitmapIndices());
            r.setFonts(((AbstractRendererHigh) ref.getRenderer()).getFonts());
            item.setModelID(MODEL_ID);
            item.setTextIds(new int[]{TEXT_LABEL + 1, TEXT_LABEL + 2, TEXT_LABEL + 3, TEXT_LABEL + 4, TEXT_LABEL + 5});
            item.setLabelId(TEXT_LABEL);
            item.setGlassplateInsetsBottom(6);
            item.setGlassplateInsetsTop(7);
            item.setInternalID(INTERNAL_ID);
            item.setType(MenuItemController.TYPE_COMBO_BOX);
            item.setWidgetID(1);

            // Views first: if this throws, the menu is untouched.
            int[] ids = screen.getViewIDs();
            HMIView[][] views = new HMIView[ids.length + 1][];
            int[] nids = new int[ids.length + 1];
            int o = 0;
            boolean placed = false;
            for (int k = 0; k < ids.length; k++) {      // findViews() binary-searches: keep ids sorted
                if (!placed && ids[k] > MODEL_ID) { nids[o] = MODEL_ID; views[o++] = new HMIView[]{item}; placed = true; }
                nids[o] = ids[k]; views[o++] = screen.findViews(ids[k]);
            }
            if (!placed) { nids[o] = MODEL_ID; views[o] = new HMIView[]{item}; }
            screen.setViews(nids, views);
            ((MenuController) parent).add(item);
        }
    }
}
