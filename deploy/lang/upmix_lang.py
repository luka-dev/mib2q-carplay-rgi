# upmix_lang.py - add the Sound effects ("Surround") and Connection manager
# ("CarPlay settings" -> "Spatial Audio" / "UpMix" + Infoline hints) texts to lang_data.zip.
# Runs on micropython (M.I.B. apps/sbin/beta) or python3; called by the
# installer (mod/custom.sh, patch_lang) with the zip path:
#
#   micropython upmix_lang.py /mnt/app/eso/hmi/lsd/jars/lang_data.zip
#
# Copyright (c) 2026 LuKa (@LuKa_dev)
import sys, os, struct
here = sys.argv[0].rsplit("/", 1)[0] if "/" in sys.argv[0] else "."
sys.path.append(here)
import ldzip

SCREEN = "HMIToneEvoHigh"
CM_SCREEN = "HMIConnectivityEvoHigh"
LABEL = "CarPlay UpMix"
OUR_LABELS = (LABEL, "UpMix")   # any wording we ever wrote: rewritten, not "taken"
SLOT = 1000          # past the stock Tone table (897 slots on US/EU/CN); text id = 1000000 + slot
CM_SLOT = 1500       # past the stock Connectivity table (~1228 slots); text id = 2500000 + slot

LEVELS = {
    "en": ["Moderate", "Normal", "Full", "Aggressive"],
    "ar": ["\u0645\u0639\u062a\u062f\u0644", "\u0639\u0627\u062f\u064a", "\u0643\u0627\u0645\u0644", "\u0645\u0643\u062b\u0641"],
    "cs": ["M\u00edrn\u00fd", "Norm\u00e1ln\u00ed", "Pln\u00fd", "Agresivn\u00ed"],
    "da": ["Moderat", "Normal", "Fuld", "Aggressiv"],
    "de": ["Moderat", "Normal", "Voll", "Aggressiv"],
    "el": ["\u039c\u03ad\u03c4\u03c1\u03b9\u03bf", "\u039a\u03b1\u03bd\u03bf\u03bd\u03b9\u03ba\u03cc", "\u03a0\u03bb\u03ae\u03c1\u03b5\u03c2", "\u0395\u03c0\u03b9\u03b8\u03b5\u03c4\u03b9\u03ba\u03cc"],
    "es": ["Moderado", "Normal", "Completo", "Agresivo"],
    "fi": ["Maltillinen", "Normaali", "T\u00e4ysi", "Aggressiivinen"],
    "fr": ["Mod\u00e9r\u00e9", "Normal", "Complet", "Agressif"],
    "hu": ["M\u00e9rs\u00e9kelt", "Norm\u00e1l", "Teljes", "Agressz\u00edv"],
    "it": ["Moderato", "Normale", "Pieno", "Aggressivo"],
    "ms": ["Sederhana", "Biasa", "Penuh", "Agresif"],
    "nb": ["Moderat", "Normal", "Full", "Aggressiv"],
    "nl": ["Gematigd", "Normaal", "Volledig", "Agressief"],
    "pl": ["Umiarkowany", "Normalny", "Pe\u0142ny", "Agresywny"],
    "pt": ["Moderado", "Normal", "Completo", "Agressivo"],
    "ro": ["Moderat", "Normal", "Complet", "Agresiv"],
    "ru": ["\u0423\u043c\u0435\u0440\u0435\u043d\u043d\u044b\u0439", "\u041e\u0431\u044b\u0447\u043d\u044b\u0439", "\u041f\u043e\u043b\u043d\u044b\u0439", "\u0410\u0433\u0440\u0435\u0441\u0441\u0438\u0432\u043d\u044b\u0439"],
    "sl": ["Zmerno", "Normalno", "Polno", "Agresivno"],
    "sv": ["M\u00e5ttlig", "Normal", "Full", "Aggressiv"],
    "tr": ["Orta", "Normal", "Tam", "Agresif"],
    "uk": ["\u041f\u043e\u043c\u0456\u0440\u043d\u0438\u0439", "\u0417\u0432\u0438\u0447\u0430\u0439\u043d\u0438\u0439", "\u041f\u043e\u0432\u043d\u0438\u0439", "\u0410\u0433\u0440\u0435\u0441\u0438\u0432\u043d\u0438\u0439"],
    "zh": ["\u9002\u4e2d", "\u6807\u51c6", "\u5b8c\u6574", "\u5f3a\u70c8"],
    "zh_HK": ["\u9069\u4e2d", "\u6a19\u6e96", "\u5b8c\u6574", "\u5f37\u70c8"],
}
OFF_SLOT = 303

SURROUND_EXTRAS = {
    "en": ["Surround", "Turn off to bypass surround processing for direct linear audio"],
    "de": ["Surround", "Ausschalten, um Surround-Verarbeitung f\u00fcr direkten Linear-Ton zu umgehen"],
    "ru": ["Surround", "\u0412\u044b\u043a\u043b\u044e\u0447\u0438\u0442\u0435 \u0434\u043b\u044f \u043f\u0440\u044f\u043c\u043e\u0439 \u043f\u0435\u0440\u0435\u0434\u0430\u0447\u0438 \u0437\u0432\u0443\u043a\u0430 \u0432 \u043e\u0431\u0445\u043e\u0434 \u043e\u0431\u0440\u0430\u0431\u043e\u0442\u043a\u0438 Surround"],
    "uk": ["Surround", "\u0412\u0438\u043c\u043a\u043d\u0456\u0442\u044c \u0434\u043b\u044f \u043f\u0440\u044f\u043c\u043e\u0457 \u043f\u0435\u0440\u0435\u0434\u0430\u0447\u0456 \u0437\u0432\u0443\u043a\u0443 \u0432 \u043e\u0431\u0445\u0456\u0434 \u043e\u0431\u0440\u043e\u0431\u043a\u0438 Surround"],
}

# Slots 1500..1524 in HMIConnectivityEvoHigh (text ids 2501500..2501524):
#  0: TEXT_TITLE                (2501500)
#  1: TEXT_SPATIAL              (2501501)
#  2: TEXT_SPATIAL_HINT         (2501502)
#  3: TEXT_UPMIX                (2501503)
#  4: TEXT_UPMIX_OFF            (2501504)
#  5: TEXT_UPMIX_NORMAL         (2501505)
#  6: TEXT_UPMIX_INTENSE        (2501506)
#  7: TEXT_UPMIX_HINT           (2501507)
#  8: TEXT_ALTSCREEN            (2501508)
#  9: TEXT_ALTSCREEN_HINT       (2501509)
# 10: TEXT_ALT_LAYOUT           (2501510)
# 11: TEXT_ALT_LAYOUT_LEFT      (2501511)
# 12: TEXT_ALT_LAYOUT_CENTER    (2501512)
# 13: TEXT_ALT_LAYOUT_RIGHT     (2501513)
# 14: TEXT_ALT_LAYOUT_HINT      (2501514)
# 15: TEXT_RGI_ENABLED          (2501515)
# 16: TEXT_RGI_ENABLED_HINT     (2501516)
# 17: TEXT_ALT_URL              (2501517)
# 18: TEXT_ALT_URL_MAP_WHEN_RGI (2501518)
# 19: TEXT_ALT_URL_CARDS_ALWAYS (2501519)
# 20: TEXT_ALT_URL_HINT         (2501520)
# 21: TEXT_RGI_FAR              (2501521)
# 22: TEXT_RGI_FAR_NEXT         (2501522)
# 23: TEXT_RGI_FAR_STRAIGHT     (2501523)
# 24: TEXT_RGI_FAR_HINT         (2501524)
CM_TEXTS = {
    "en": [
        "CarPlay settings",
        "Spatial Audio",
        "Play multichannel 5.1 audio in supported apps",
        "UpMix",
        "Off",
        "Normal",
        "Intense",
        "Expand stereo audio into 5.1 surround sound",
        "Maps on Cluster",
        "Show CarPlay maps on the instrument cluster",
        "Maneuver Cards Position",
        "Left",
        "Center",
        "Right",
        "Choose where turn cards and arrival time appear on the map",
        "Route Guidance",
        "Show turn-by-turn directions on the cluster and Head-Up Display",
        "Map Cards during Guidance",
        "Hidden",
        "Shown",
        "Hide CarPlay turn cards on the map when Route Guidance is active",
        "Guidance Maneuver Preview",
        "Always next maneuver",
        "Follow road when far",
        "Show the next turn in advance, or a straight arrow until you get closer",
    ],
    "de": [
        "CarPlay-Einstellungen",
        "Spatial Audio",
        "Mehrkanal-Audio (5.1) in unterst\u00fctzten Apps wiedergeben",
        "UpMix",
        "Aus",
        "Normal",
        "Intensiv",
        "Stereoton auf 5.1-Surround-Sound erweitern",
        "Karten im Kombiinstrument",
        "CarPlay-Karten im Kombiinstrument anzeigen",
        "Position der Man\u00f6verkarten",
        "Links",
        "Mitte",
        "Rechts",
        "Position f\u00fcr Abbiegekarten und Ankunftszeit auf der Karte w\u00e4hlen",
        "Zielf\u00fchrung",
        "Abbiegehinweise im Kombiinstrument und Head-up-Display anzeigen",
        "Karten-Einblendungen bei Zielf\u00fchrung",
        "Ausgeblendet",
        "Angezeigt",
        "CarPlay-Abbiegekarten auf der Karte bei aktiver Zielf\u00fchrung ausblenden",
        "Man\u00f6vervorschau bei Zielf\u00fchrung",
        "Immer n\u00e4chstes Man\u00f6ver",
        "Geradeauspfeil wenn weit entfernt",
        "N\u00e4chste Abbiegung vorab anzeigen oder Geradeauspfeil bis kurz davor",
    ],
    "ru": [
        "\u041d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 CarPlay",
        "Spatial Audio",
        "\u0412\u043e\u0441\u043f\u0440\u043e\u0438\u0437\u0432\u043e\u0434\u0438\u0442\u044c \u043c\u043d\u043e\u0433\u043e\u043a\u0430\u043d\u0430\u043b\u044c\u043d\u044b\u0439 \u0437\u0432\u0443\u043a 5.1 \u0432 \u043f\u043e\u0434\u0434\u0435\u0440\u0436\u0438\u0432\u0430\u0435\u043c\u044b\u0445 \u043f\u0440\u0438\u043b\u043e\u0436\u0435\u043d\u0438\u044f\u0445",
        "UpMix",
        "\u0412\u044b\u043a\u043b.",
        "\u041e\u0431\u044b\u0447\u043d\u044b\u0439",
        "\u0418\u043d\u0442\u0435\u043d\u0441\u0438\u0432\u043d\u044b\u0439",
        "\u041f\u0440\u0435\u043e\u0431\u0440\u0430\u0437\u043e\u0432\u044b\u0432\u0430\u0442\u044c \u0441\u0442\u0435\u0440\u0435\u043e\u0437\u0432\u0443\u043a \u0432 \u043e\u0431\u044a\u0451\u043c\u043d\u043e\u0435 \u0437\u0432\u0443\u0447\u0430\u043d\u0438\u0435 5.1",
        "\u041a\u0430\u0440\u0442\u044b \u043d\u0430 \u043f\u0440\u0438\u0431\u043e\u0440\u043d\u043e\u0439 \u043f\u0430\u043d\u0435\u043b\u0438",
        "\u041f\u043e\u043a\u0430\u0437\u044b\u0432\u0430\u0442\u044c \u043a\u0430\u0440\u0442\u044b CarPlay \u043d\u0430 \u043f\u0440\u0438\u0431\u043e\u0440\u043d\u043e\u0439 \u043f\u0430\u043d\u0435\u043b\u0438",
        "\u041f\u043e\u0437\u0438\u0446\u0438\u044f \u043a\u0430\u0440\u0442\u043e\u0447\u0435\u043a \u043c\u0430\u043d\u0451\u0432\u0440\u043e\u0432",
        "\u0421\u043b\u0435\u0432\u0430",
        "\u041f\u043e \u0446\u0435\u043d\u0442\u0440\u0443",
        "\u0421\u043f\u0440\u0430\u0432\u0430",
        "\u0412\u044b\u0431\u0435\u0440\u0438\u0442\u0435, \u0433\u0434\u0435 \u043d\u0430 \u043a\u0430\u0440\u0442\u0435 \u043f\u043e\u043a\u0430\u0437\u044b\u0432\u0430\u0442\u044c \u043c\u0430\u043d\u0451\u0432\u0440\u044b \u0438 \u0432\u0440\u0435\u043c\u044f \u043f\u0440\u0438\u0431\u044b\u0442\u0438\u044f",
        "\u0412\u0435\u0434\u0435\u043d\u0438\u0435 \u043f\u043e \u043c\u0430\u0440\u0448\u0440\u0443\u0442\u0443",
        "\u041f\u043e\u043a\u0430\u0437\u044b\u0432\u0430\u0442\u044c \u043c\u0430\u043d\u0451\u0432\u0440\u044b \u043d\u0430 \u043f\u0440\u0438\u0431\u043e\u0440\u043d\u043e\u0439 \u043f\u0430\u043d\u0435\u043b\u0438 \u0438 \u043f\u0440\u043e\u0435\u043a\u0446\u0438\u043e\u043d\u043d\u043e\u043c \u0434\u0438\u0441\u043f\u043b\u0435\u0435",
        "\u041a\u0430\u0440\u0442\u043e\u0447\u043a\u0438 \u043d\u0430 \u043a\u0430\u0440\u0442\u0435 \u043f\u0440\u0438 \u0432\u0435\u0434\u0435\u043d\u0438\u0438",
        "\u0421\u043a\u0440\u044b\u0442\u044b",
        "\u041f\u043e\u043a\u0430\u0437\u0430\u043d\u044b",
        "\u0421\u043a\u0440\u044b\u0432\u0430\u0442\u044c \u043a\u0430\u0440\u0442\u043e\u0447\u043a\u0438 \u043c\u0430\u043d\u0451\u0432\u0440\u043e\u0432 CarPlay \u043d\u0430 \u043a\u0430\u0440\u0442\u0435, \u043a\u043e\u0433\u0434\u0430 \u0430\u043a\u0442\u0438\u0432\u043d\u043e \u0432\u0435\u0434\u0435\u043d\u0438\u0435",
        "\u041f\u0440\u0435\u0434\u043f\u0440\u043e\u0441\u043c\u043e\u0442\u0440 \u0441\u043b\u0435\u0434\u0443\u044e\u0449\u0435\u0433\u043e \u043c\u0430\u043d\u0451\u0432\u0440\u0430",
        "\u0412\u0441\u0435\u0433\u0434\u0430 \u0441\u043b\u0435\u0434\u0443\u044e\u0449\u0438\u0439 \u043c\u0430\u043d\u0451\u0432\u0440",
        "\u0421\u0442\u0440\u0435\u043b\u043a\u0430 \u043f\u0440\u044f\u043c\u043e, \u0435\u0441\u043b\u0438 \u0434\u0430\u043b\u0435\u043a\u043e",
        "\u041f\u043e\u043a\u0430\u0437\u044b\u0432\u0430\u0442\u044c \u0441\u043b\u0435\u0434\u0443\u044e\u0449\u0438\u0439 \u043f\u043e\u0432\u043e\u0440\u043e\u0442 \u0437\u0430\u0440\u0430\u043d\u0435\u0435 \u0438\u043b\u0438 \u0441\u0442\u0440\u0435\u043b\u043a\u0443 \u043f\u0440\u044f\u043c\u043e \u0434\u043e \u0441\u0431\u043b\u0438\u0436\u0435\u043d\u0438\u044f",
    ],
    "uk": [
        "\u041d\u0430\u043b\u0430\u0448\u0442\u0443\u0432\u0430\u043d\u043d\u044f CarPlay",
        "Spatial Audio",
        "\u0412\u0456\u0434\u0442\u0432\u043e\u0440\u044e\u0432\u0430\u0442\u0438 \u0431\u0430\u0433\u0430\u0442\u043e\u043a\u0430\u043d\u0430\u043b\u044c\u043d\u0438\u0439 \u0437\u0432\u0443\u043a 5.1 \u0443 \u043f\u0456\u0434\u0442\u0440\u0438\u043c\u0443\u0432\u0430\u043d\u0438\u0445 \u0434\u043e\u0434\u0430\u0442\u043a\u0430\u0445",
        "UpMix",
        "\u0412\u0438\u043c\u043a.",
        "\u0417\u0432\u0438\u0447\u0430\u0439\u043d\u0438\u0439",
        "\u0406\u043d\u0442\u0435\u043d\u0441\u0438\u0432\u043d\u0438\u0439",
        "\u041f\u0435\u0440\u0435\u0442\u0432\u043e\u0440\u044e\u0432\u0430\u0442\u0438 \u0441\u0442\u0435\u0440\u0435\u043e\u0437\u0432\u0443\u043a \u0432 \u043e\u0431'\u0454\u043c\u043d\u0435 \u0437\u0432\u0443\u0447\u0430\u043d\u043d\u044f 5.1",
        "\u041a\u0430\u0440\u0442\u0438 \u043d\u0430 \u043f\u0430\u043d\u0435\u043b\u0456 \u043f\u0440\u0438\u043b\u0430\u0434\u0456\u0432",
        "\u041f\u043e\u043a\u0430\u0437\u0443\u0432\u0430\u0442\u0438 \u043a\u0430\u0440\u0442\u0438 CarPlay \u043d\u0430 \u043f\u0430\u043d\u0435\u043b\u0456 \u043f\u0440\u0438\u043b\u0430\u0434\u0456\u0432",
        "\u041f\u043e\u0437\u0438\u0446\u0456\u044f \u043a\u0430\u0440\u0442\u043e\u043a \u043c\u0430\u043d\u0435\u0432\u0440\u0456\u0432",
        "\u041b\u0456\u0432\u043e\u0440\u0443\u0447",
        "\u041f\u043e \u0446\u0435\u043d\u0442\u0440\u0443",
        "\u041f\u0440\u0430\u0432\u043e\u0440\u0443\u0447",
        "\u0412\u0438\u0431\u0435\u0440\u0456\u0442\u044c, \u0434\u0435 \u043d\u0430 \u043a\u0430\u0440\u0442\u0456 \u043f\u043e\u043a\u0430\u0437\u0443\u0432\u0430\u0442\u0438 \u043c\u0430\u043d\u0435\u0432\u0440\u0438 \u0442\u0430 \u0447\u0430\u0441 \u043f\u0440\u0438\u0431\u0443\u0442\u0442\u044f",
        "\u0412\u0435\u0434\u0435\u043d\u043d\u044f \u043f\u043e \u043c\u0430\u0440\u0448\u0440\u0443\u0442\u0443",
        "\u041f\u043e\u043a\u0430\u0437\u0443\u0432\u0430\u0442\u0438 \u043c\u0430\u043d\u0435\u0432\u0440\u0438 \u043d\u0430 \u043f\u0430\u043d\u0435\u043b\u0456 \u043f\u0440\u0438\u043b\u0430\u0434\u0456\u0432 \u0456 \u043f\u0440\u043e\u0435\u043a\u0446\u0456\u0439\u043d\u043e\u043c\u0443 \u0434\u0438\u0441\u043f\u043b\u0435\u0457",
        "\u041a\u0430\u0440\u0442\u043a\u0438 \u043d\u0430 \u043a\u0430\u0440\u0442\u0456 \u043f\u0456\u0434 \u0447\u0430\u0441 \u0432\u0435\u0434\u0435\u043d\u043d\u044f",
        "\u041f\u0440\u0438\u0445\u043e\u0432\u0430\u043d\u0456",
        "\u041f\u043e\u043a\u0430\u0437\u0430\u043d\u0456",
        "\u041f\u0440\u0438\u0445\u043e\u0432\u0443\u0432\u0430\u0442\u0438 \u043a\u0430\u0440\u0442\u043a\u0438 \u043c\u0430\u043d\u0435\u0432\u0440\u0456\u0432 CarPlay \u043d\u0430 \u043a\u0430\u0440\u0442\u0456 \u043f\u0456\u0434 \u0447\u0430\u0441 \u0430\u043a\u0442\u0438\u0432\u043d\u043e\u0433\u043e \u0432\u0435\u0434\u0435\u043d\u043d\u044f",
        "\u041f\u043e\u043f\u0435\u0440\u0435\u0434\u043d\u0456\u0439 \u043f\u0435\u0440\u0435\u0433\u043b\u044f\u0434 \u043c\u0430\u043d\u0435\u0432\u0440\u0443",
        "\u0417\u0430\u0432\u0436\u0434\u0438 \u043d\u0430\u0441\u0442\u0443\u043f\u043d\u0438\u0439 \u043c\u0430\u043d\u0435\u0432\u0440",
        "\u0421\u0442\u0440\u0456\u043b\u043a\u0430 \u043f\u0440\u044f\u043c\u043e, \u044f\u043a\u0449\u043e \u0434\u0430\u043b\u0435\u043a\u043e",
        "\u041f\u043e\u043a\u0430\u0437\u0443\u0432\u0430\u0442\u0438 \u043d\u0430\u0441\u0442\u0443\u043f\u043d\u0438\u0439 \u043f\u043e\u0432\u043e\u0440\u043e\u0442 \u0437\u0430\u0437\u0434\u0430\u043b\u0435\u0433\u0456\u0434\u044c \u0430\u0431\u043e \u0441\u0442\u0440\u0456\u043b\u043a\u0443 \u043f\u0440\u044f\u043c\u043e \u0434\u043e \u043d\u0430\u0431\u043b\u0438\u0436\u0435\u043d\u043d\u044f",
    ],
    "es": [
        "Ajustes de CarPlay",
        "Spatial Audio",
        "Reproduce audio multicanal 5.1 en las apps compatibles",
        "UpMix",
        "Desactivado",
        "Normal",
        "Intenso",
        "Convierte el audio est\u00e9reo en sonido envolvente 5.1",
        "Mapas en el cuadro",
        "Muestra los mapas de CarPlay en el cuadro de instrumentos",
        "Posici\u00f3n de tarjetas de maniobra",
        "Izquierda",
        "Centro",
        "Derecha",
        "Elige d\u00f3nde mostrar las indicaciones y la hora de llegada en el mapa",
        "Gu\u00eda al destino",
        "Muestra las indicaciones giro a giro en el cuadro y el Head-Up Display",
        "Tarjetas en el mapa durante la gu\u00eda",
        "Ocultas",
        "Visibles",
        "Oculta las tarjetas de CarPlay en el mapa cuando la gu\u00eda est\u00e1 activa",
        "Vista previa de maniobra",
        "Siempre siguiente maniobra",
        "Flecha recto si est\u00e1 lejos",
        "Muestra el pr\u00f3ximo giro por adelantado o una flecha recto hasta acercarte",
    ],
    "fr": [
        "R\u00e9glages CarPlay",
        "Spatial Audio",
        "Lire l'audio multicanal 5.1 dans les apps compatibles",
        "UpMix",
        "D\u00e9sactiv\u00e9",
        "Normal",
        "Intense",
        "Convertir l'audio st\u00e9r\u00e9o en son surround 5.1",
        "Cartes sur le combin\u00e9",
        "Afficher les cartes CarPlay sur le combin\u00e9 d'instruments",
        "Position des cartes de man\u0153uvre",
        "Gauche",
        "Centre",
        "Droite",
        "Choisir o\u00f9 afficher les consignes et l'heure d'arriv\u00e9e sur la carte",
        "Guidage routier",
        "Afficher les instructions de navigation sur le combin\u00e9 et l'affichage t\u00eate haute",
        "Cartes sur la carte pendant le guidage",
        "Masqu\u00e9es",
        "Affich\u00e9es",
        "Masquer les cartes de man\u0153uvre CarPlay sur la carte pendant le guidage",
        "Aper\u00e7u des man\u0153uvres",
        "Toujours la prochaine man\u0153uvre",
        "Fl\u00e8che tout droit si loin",
        "Afficher le prochain virage \u00e0 l'avance ou une fl\u00e8che tout droit avant d'approcher",
    ],
    "it": [
        "Impostazioni CarPlay",
        "Spatial Audio",
        "Riproduce audio multicanale 5.1 nelle app compatibili",
        "UpMix",
        "Off",
        "Normale",
        "Intenso",
        "Converte l'audio stereo in audio surround 5.1",
        "Mappe sul quadro strumenti",
        "Mostra le mappe di CarPlay sul quadro strumenti",
        "Posizione schede manovra",
        "Sinistra",
        "Centro",
        "Destra",
        "Scegli dove mostrare le indicazioni e l'ora di arrivo sulla mappa",
        "Guida a destinazione",
        "Mostra le indicazioni di svolta sul quadro e sull'Head-Up Display",
        "Schede mappa durante la guida",
        "Nascoste",
        "Mostrate",
        "Nasconde le schede di svolta CarPlay sulla mappa durante la guida",
        "Anteprima manovra",
        "Sempre prossima manovra",
        "Freccia dritto se lontano",
        "Mostra la prossima svolta in anticipo o una freccia dritto finch\u00e9 ti avvicini",
    ],
    "nl": [
        "CarPlay-instellingen",
        "Spatial Audio",
        "Speel meerkanaals 5.1-audio af in ondersteunde apps",
        "UpMix",
        "Uit",
        "Normaal",
        "Intens",
        "Zet stereogeluid om naar 5.1 surround sound",
        "Kaarten op instrumentenpaneel",
        "Toon CarPlay-kaarten op het instrumentenpaneel",
        "Positie manoeuvrekaarten",
        "Links",
        "Midden",
        "Rechts",
        "Kies waar afslagkaarten en aankomsttijd op de kaart verschijnen",
        "Routebegeleiding",
        "Toon navigatie-aanwijzingen op het instrumentenpaneel en Head-Up Display",
        "Kaartmeldingen tijdens begeleiding",
        "Verborgen",
        "Weergegeven",
        "Verberg CarPlay-afslagkaarten op de kaart tijdens actieve begeleiding",
        "Manoevrevoorvertoning",
        "Altijd volgende manoeuvre",
        "Pijl rechtdoor indien ver",
        "Toon de volgende afslag vooraf of een pijl rechtdoor tot je dichterbij bent",
    ],
    "pl": [
        "Ustawienia CarPlay",
        "Spatial Audio",
        "Odtwarzaj wielokana\u0142owy d\u017awi\u0119k 5.1 w obs\u0142ugiwanych aplikacjach",
        "UpMix",
        "Wy\u0142.",
        "Normalny",
        "Intensywny",
        "Przekszta\u0142caj d\u017awi\u0119k stereo w d\u017awi\u0119k przestrzenny 5.1",
        "Mapy na zestawie wska\u017anik\u00f3w",
        "Pokazuj mapy CarPlay na zestawie wska\u017anik\u00f3w",
        "Pozycja kart manewr\u00f3w",
        "Po lewej",
        "Na \u015brodku",
        "Po prawej",
        "Wybierz miejsce wy\u015bwietlania manewr\u00f3w i czasu przyjazdu na mapie",
        "Prowadzenie do celu",
        "Pokazuj wskaz\u00f3wki nawigacji na zestawie wska\u017anik\u00f3w i wy\u015bwietlaczu HUD",
        "Karty na mapie podczas prowadzenia",
        "Ukryte",
        "Widoczne",
        "Ukrywaj karty manewr\u00f3w CarPlay na mapie podczas aktywnego prowadzenia",
        "Podgl\u0105d nast\u0119pnego manewru",
        "Zawsze nast\u0119pny manewr",
        "Strza\u0142ka prosto, gdy daleko",
        "Pokazuj nast\u0119pny skr\u0119t z wyprzedzeniem lub strza\u0142k\u0119 prosto do zbli\u017cenia si\u0119",
    ],
    "pt": [
        "Defini\u00e7\u00f5es do CarPlay",
        "Spatial Audio",
        "Reproduz \u00e1udio multicanal 5.1 nas aplica\u00e7\u00f5es compat\u00edveis",
        "UpMix",
        "Desligado",
        "Normal",
        "Intenso",
        "Converte o \u00e1udio est\u00e9reo em som surround 5.1",
        "Mapas no painel de instrumentos",
        "Mostra os mapas do CarPlay no painel de instrumentos",
        "Posi\u00e7\u00e3o dos cart\u00f5es de manobra",
        "Esquerda",
        "Centro",
        "Direita",
        "Escolha onde mostrar as indica\u00e7\u00f5es e a hora de chegada no mapa",
        "Orienta\u00e7\u00e3o de rota",
        "Mostra indica\u00e7\u00f5es de navega\u00e7\u00e3o no painel e no Head-Up Display",
        "Cart\u00f5es no mapa durante a orienta\u00e7\u00e3o",
        "Ocultos",
        "Vis\u00edveis",
        "Oculta os cart\u00f5es de manobra do CarPlay no mapa durante a orienta\u00e7\u00e3o",
        "Pr\u00e9-visualiza\u00e7\u00e3o de manobra",
        "Sempre a pr\u00f3xima manobra",
        "Seta em frente quando longe",
        "Mostra a pr\u00f3xima curva antecipadamente ou uma seta em frente at\u00e9 se aproximar",
    ],
    "cs": [
        "Nastaven\u00ed CarPlay",
        "Spatial Audio",
        "P\u0159ehr\u00e1vat v\u00edcekan\u00e1lov\u00fd zvuk 5.1 v podporovan\u00fdch aplikac\u00edch",
        "UpMix",
        "Vyp.",
        "Norm\u00e1ln\u00ed",
        "Intenzivn\u00ed",
        "P\u0159ev\u00e1d\u011bt stereofonn\u00ed zvuk na prostorov\u00fd zvuk 5.1",
        "Mapy v p\u0159\u00edstrojov\u00e9m \u0161t\u00edtu",
        "Zobrazovat mapy CarPlay na p\u0159\u00edstrojov\u00e9m \u0161t\u00edtu",
        "Pozice karet man\u00e9vr\u016f",
        "Vlevo",
        "Uprost\u0159ed",
        "Vpravo",
        "Vyberte um\u00edst\u011bn\u00ed karet man\u00e9vr\u016f a \u010dasu p\u0159\u00edjezdu na map\u011b",
        "Nav\u00e1d\u011bn\u00ed k c\u00edli",
        "Zobrazovat pokyny navigace na p\u0159\u00edstrojov\u00e9m \u0161t\u00edtu a displeji HUD",
        "Karty na map\u011b b\u011bhem nav\u00e1d\u011bn\u00ed",
        "Skryt\u00e9",
        "Zobrazen\u00e9",
        "Skr\u00fdt karty man\u00e9vr\u016f CarPlay na map\u011b p\u0159i aktivn\u00edm nav\u00e1d\u011bn\u00ed",
        "N\u00e1hled n\u00e1sleduj\u00edc\u00edho man\u00e9vru",
        "V\u017edy dal\u0161\u00ed man\u00e9vr",
        "\u0160ipka rovn\u011b, kdy\u017e je daleko",
        "Zobrazit dal\u0161\u00ed odbo\u010dku p\u0159edem, nebo \u0161ipku rovn\u011b a\u017e do p\u0159ibl\u00ed\u017een\u00ed",
    ],
    "tr": [
        "CarPlay ayarlar\u0131",
        "Spatial Audio",
        "Desteklenen uygulamalarda \u00e7ok kanall\u0131 5.1 ses \u00e7alar",
        "UpMix",
        "Kapal\u0131",
        "Normal",
        "Yo\u011fun",
        "Stereo sesi 5.1 \u00e7evresel sese d\u00f6n\u00fc\u015ft\u00fcr\u00fcr",
        "G\u00f6sterge panelinde haritalar",
        "CarPlay haritalar\u0131n\u0131 g\u00f6sterge panelinde g\u00f6sterir",
        "Manevra kartlar\u0131 konumu",
        "Sol",
        "Orta",
        "Sa\u011f",
        "Haritada d\u00f6n\u00fc\u015f kartlar\u0131n\u0131n ve var\u0131\u015f s\u00fcresinin konumunu se\u00e7in",
        "Rota rehberli\u011fi",
        "D\u00f6n\u00fc\u015f y\u00f6nlendirmelerini g\u00f6sterge panelinde ve Head-Up Display'de g\u00f6sterir",
        "Rehberlik s\u0131ras\u0131nda harita kartlar\u0131",
        "Gizli",
        "G\u00f6r\u00fcn\u00fcr",
        "Rota rehberli\u011fi etkinken haritadaki CarPlay d\u00f6n\u00fc\u015f kartlar\u0131n\u0131 gizler",
        "Manevra \u00f6nizlemesi",
        "Her zaman sonraki manevra",
        "Uzaktayken d\u00fcz ok",
        "Sonraki d\u00f6n\u00fc\u015f\u00fc \u00f6nceden veya d\u00f6n\u00fc\u015fe yakla\u015fana kadar d\u00fcz ok g\u00f6sterir",
    ],
    "zh": [
        "CarPlay \u8bbe\u7f6e",
        "\u7a7a\u95f4\u97f3\u9891 (Spatial Audio)",
        "\u5728\u652f\u6301\u7684\u5e94\u7528\u4e2d\u64ad\u653e 5.1 \u591a\u58f0\u9053\u97f3\u9891",
        "UpMix \u73af\u7ed5\u4e0a\u6df7",
        "\u5173\u95ed",
        "\u6807\u51c6",
        "\u5f3a\u70c8",
        "\u5c06\u7acb\u4f53\u58f0\u97f3\u9891\u6269\u5c55\u4e3a 5.1 \u73af\u7ed5\u58f0",
        "\u4eea\u8868\u76d8\u5730\u56fe",
        "\u5728\u4eea\u8868\u76d8\u4e0a\u663e\u793a CarPlay \u5730\u56fe",
        "\u5bfc\u822a\u5361\u7247\u4f4d\u7f6e",
        "\u5de6\u4fa7",
        "\u5c45\u4e2d",
        "\u53f3\u4fa7",
        "\u9009\u62e9\u5bfc\u822a\u5361\u7247\u548c\u5230\u8fbe\u65f6\u95f4\u5728\u5730\u56fe\u4e0a\u7684\u663e\u793a\u4f4d\u7f6e",
        "\u8def\u7ebf\u5f15\u5bfc",
        "\u5728\u4eea\u8868\u76d8\u548c\u62ac\u5934\u663e\u793a\u4e0a\u663e\u793a\u5bfc\u822a\u6307\u793a",
        "\u5bfc\u822a\u65f6\u5730\u56fe\u5361\u7247",
        "\u9690\u85cf",
        "\u663e\u793a",
        "\u8def\u7ebf\u5f15\u5bfc\u5f00\u542f\u65f6\u9690\u85cf\u5730\u56fe\u4e0a\u7684 CarPlay \u5bfc\u822a\u5361\u7247",
        "\u5bfc\u822a\u8f6c\u5411\u9884\u89c8",
        "\u59cb\u7ec8\u663e\u793a\u4e0b\u4e00\u8f6c\u5411",
        "\u8ddd\u79bb\u8f83\u8fdc\u65f6\u663e\u793a\u76f4\u884c",
        "\u63d0\u524d\u663e\u793a\u4e0b\u4e00\u8f6c\u5411\uff0c\u6216\u5728\u63a5\u8fd1\u8def\u53e3\u524d\u663e\u793a\u76f4\u884c\u7bad\u5934",
    ],
    "zh_HK": [
        "CarPlay \u8a2d\u5b9a",
        "\u7a7a\u9593\u97f3\u8a0a (Spatial Audio)",
        "\u5728\u652f\u63f4\u7684 App \u4e2d\u64ad\u653e 5.1 \u591a\u8072\u9053\u97f3\u8a0a",
        "UpMix \u74b0\u7e5e\u4e0a\u6df7",
        "\u95dc\u9589",
        "\u6a19\u6e96",
        "\u5f37\u70c8",
        "\u5c07\u7acb\u9ad4\u8072\u97f3\u8a0a\u64f4\u5c55\u70ba 5.1 \u74b0\u7e5e\u8072",
        "\u5100\u9336\u677f\u5730\u5716",
        "\u5728\u5100\u9336\u677f\u4e0a\u986f\u793a CarPlay \u5730\u5716",
        "\u5c0e\u822a\u5361\u7247\u4f4d\u7f6e",
        "\u5de6\u5074",
        "\u7f6e\u4e2d",
        "\u53f3\u5074",
        "\u9078\u64c7\u5c0e\u822a\u5361\u7247\u548c\u5230\u9054\u6642\u9593\u5728\u5730\u5716\u4e0a\u7684\u986f\u793a\u4f4d\u7f6e",
        "\u8def\u7dda\u5f15\u5c0e",
        "\u5728\u5100\u9336\u677f\u548c\u62ac\u982d\u986f\u793a\u5668\u4e0a\u986f\u793a\u5c0e\u822a\u6307\u793a",
        "\u5c0e\u822a\u6642\u5730\u5716\u5361\u7247",
        "\u96b1\u85cf",
        "\u986f\u793a",
        "\u8def\u7dda\u5f15\u5c0e\u958b\u555f\u6642\u96b1\u85cf\u5730\u5716\u4e0a\u7684 CarPlay \u5c0e\u822a\u5361\u7247",
        "\u5c0e\u822a\u8f49\u5411\u9810\u89bd",
        "\u59cb\u7d42\u986f\u793a\u4e0b\u4e00\u8f49\u5411",
        "\u8ddd\u96e2\u8f03\u9060\u6642\u986f\u793a\u76f4\u884c",
        "\u63d0\u524d\u986f\u793a\u4e0b\u4e00\u8f49\u5411\uff0c\u6216\u5728\u63a5\u8fd1\u8def\u53e3\u524d\u986f\u793a\u76f4\u884c\u7bad\u982d",
    ],
}
CM_OUR_LABELS = tuple(v[0] for v in CM_TEXTS.values())

def texts(loc, strs):
    levels = LEVELS.get(loc) or LEVELS.get(loc[:2]) or LEVELS["en"]
    surround = SURROUND_EXTRAS.get(loc) or SURROUND_EXTRAS.get(loc[:2]) or SURROUND_EXTRAS["en"]
    off = strs[OFF_SLOT] if len(strs) > OFF_SLOT and strs[OFF_SLOT] else "Off"
    return [LABEL, off] + levels + surround

def cm_texts(loc):
    return list(CM_TEXTS.get(loc) or CM_TEXTS.get(loc[:2]) or CM_TEXTS["en"])

def zip_write_to(f, names, meta, raw):
    """ldzip.zip_write, streamed: the same bytes without building the archive in
    memory (the EU zip is 4.2 MB; the in-memory concat ran out of heap)."""
    central = []; off = 0
    for name in names:
        method, crc, usize = meta[name]; body = raw[name]; nm = name.encode("utf-8"); csize = len(body)
        lh = (b"PK\x03\x04" + struct.pack("<HHHHH", 20, 0, method, 0, 0)
              + struct.pack("<III", crc, csize, usize) + struct.pack("<HH", len(nm), 0) + nm)
        f.write(lh); f.write(body)
        central.append(b"PK\x01\x02" + struct.pack("<HHHHHH", 20, 20, 0, method, 0, 0)
                       + struct.pack("<III", crc, csize, usize)
                       + struct.pack("<HHHHH", len(nm), 0, 0, 0, 0)
                       + struct.pack("<II", 0, off) + nm)
        off += len(lh) + csize
    clen = 0
    for c in central:
        f.write(c); clen += len(c)
    f.write(b"PK\x05\x06" + struct.pack("<HH", 0, 0) + struct.pack("<HH", len(names), len(names))
            + struct.pack("<I", clen) + struct.pack("<I", off) + struct.pack("<H", 0))

def patch_table(n, loc, slot, want, our_labels, meta, raw):
    m = meta[n][0]
    strs, flags = ldzip.d_unpack(ldzip.inflate(raw[n]) if m == 8 else raw[n])
    if callable(want):
        want = want(loc, strs)
    have = strs[slot:slot + len(want)]
    if have == want:
        return (0, 1, 0)
    if (not have or have[0] not in our_labels) and [t for t in have if t]:
        print("UpMix texts: %s slots %d.. hold other text %r - skipped" % (loc, slot, have))
        return (0, 0, 1)
    flags = bytearray(flags)
    while len(strs) < slot + len(want):
        strs.append(""); flags.append(0)
    for k in range(len(want)):
        strs[slot + k] = want[k]; flags[slot + k] = 8
    d = ldzip.d_pack(strs, flags)
    raw[n] = d; meta[n] = (0, ldzip.crc32(d), len(d))
    return (1, 0, 0)

def patch(zp):
    names, meta, raw = ldzip.zip_read(open(zp, "rb").read())
    pre_tone = "resources/%s/strings_" % SCREEN
    pre_cm   = "resources/%s/strings_" % CM_SCREEN
    added = present = taken = 0
    for n in names:
        if n.startswith(pre_tone) and n.endswith(".data"):
            loc = n[len(pre_tone):-len(".data")]
            a, p, t = patch_table(n, loc, SLOT, texts, OUR_LABELS, meta, raw)
            added += a; present += p; taken += t
        elif n.startswith(pre_cm) and n.endswith(".data"):
            loc = n[len(pre_cm):-len(".data")]
            a, p, t = patch_table(n, loc, CM_SLOT, cm_texts(loc), CM_OUR_LABELS, meta, raw)
            added += a; present += p; taken += t
    if added + present + taken == 0:
        print("UpMix texts: no %s tables in %s" % (SCREEN, zp)); return 1
    if added:
        tmp = zp + ".carplay-new"
        f = open(tmp, "wb"); zip_write_to(f, names, meta, raw); f.close()
        os.rename(tmp, zp)
    print("UpMix texts: added to %d locale(s), already in %d, slots taken in %d" % (added, present, taken))
    return 1 if taken else 0

if __name__ == "__main__":
    sys.exit(patch(sys.argv[1]))
