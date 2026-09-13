# Audi material-inspired bases with fixed blue fill — 2026-09-13

User asked to look further in Audi's palette for ONLY the base colour. Existing
full fill / blink-on MUST remain blue; no white active endpoint. Grey/silver bases
were rejected, and red was questioned as resembling a wrong manoeuvre.

This study explicitly broadens inspiration to Audi interior materials. It does
NOT claim to extract factory VC UI colours or exact paint/leather RGB values.
The named material directions are documented by Audi; the shader values are our
own design adaptations for preview (not sampled or calibrated colour matches):
A Neodymium Gold-inspired #C3A05D; B A1 copper-inspired #C57B46;
C Mora Violet-inspired #906B9D.

Primary sources:
- Audi exclusive A6 Avant, 2025-04-08, Neodymium Gold and Mora Violet:
  https://www.audi.com/en/press-releases/audi-exclusive-unveils-upgraded-a6-avant-at-milan-design-week-16593
- Audi A1 Sportback press information, p21, copper interior accents/stitching:
  https://uploads.audi-mediacenter.com/system/production/uploaded_files/14742/file/f99bd37aff627605544513b83a411a86778ff4ff/20190124_AMC_ePresseInfo_Audi_A1_Sportback.pdf?1560258932=&disposition=attachment

The production active shader line vec3 ice = vec3(0.32, 0.77, 1.0) is untouched.
100% / blink-on images for all three manoeuvres and all candidates are PIXEL
IDENTICAL to the preceding fixed-blue study. All 27 actual GL captures passed the
premultiplied-alpha check without GL errors. Active host luminance is above the
base in each pair; the physical cluster is unverified. The neutral #464646 backdrop
is a contrast probe, not a calibrated cluster screenshot.

base-comparison.png and uturn-comparison.png show 0% and 50%. Sampled host pixels
and shader inputs are recorded in palette-values.json. Two endpoints and the
existing colour interpolation, feathered front and animations are unchanged.
No base has been selected and no production change/commit/deployment was made.
Temporary sources/build/raw captures: build/audi-material-base-study/.
