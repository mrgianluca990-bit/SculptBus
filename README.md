# Sculpt Bus v0.1

First bus-oriented derivative of Sculpt Channel 1.0.

## Goal
Same four-macro concept, but optimised for subgroups, mix bus and gentle stem processing.

## DSP differences from Sculpt Channel 1.0
- broader macro filters
- lower maximum EQ excursion
- slower, lower-ratio stereo-linked glue compression
- substantially less saturation, entering mainly near the top of positive travel
- resonance detector uses broader Q, lower VAR sensitivity and softer damping
- slower RES gain movement to avoid pumping or comb-like sensations
- 4x oversampling retained

## Current GUI
Uses the approved Sculpt Channel hardware layout as the common family base with a temporary BUS identifier. Dedicated BUS artwork can be finalised after the first listening pass.

## Build formats
- macOS: VST3 / AU / Standalone
- Windows: VST3 x64 / Standalone
- AAX later


## v0.2 Fine Tune

The v0.1 sound is preserved as the base character.

Added the finishing controls from the earlier Finish concept:
- Density
- Body
- Detail
- Glue
- Punch
- Space
- Mix

Changes:
- GUI identity now says BUS instead of CHANNEL.
- A dedicated lower Fine Tune rack strip has been added.
- GLUE is now a true stereo-linked bus compressor stage.
- PUNCH changes the bus-compressor attack rather than acting as an aggressive transient shaper.
- Body and Detail are broad ±2 dB tone contours.
- Space is intentionally limited to ±12% width.
- Density is a subtle full-range soft saturation stage.
- Mix blends the entire Sculpt Bus processing in parallel.
- Existing per-band compression was reduced so the global bus compressor is the main glue stage.


## v0.2.1 GUI implemented
- Rebuilt the BUS artwork as the real GUI basis.
- The background now truly says BUS and includes the lower finish controls in the artwork.
- Control wells were cleaned so the live JUCE knobs and meters align with the new image instead of colliding with printed controls underneath.
- DSP remains the same as v0.2 Fine Tune.


## v0.2.2 GUI precise alignment
- Rechecked all knob centres against the BUS artwork and corrected the bounds of every control.
- Top 4 macro knobs, output knob, VAR switch, and all 7 lower fine-tune knobs were repositioned.
- Meter windows were also nudged for tighter alignment.
- DSP unchanged from v0.2 Fine Tune / v0.2.1.


## v0.3 Post Fine Tune
- Four macro bands are unchanged from the approved v0.2.2 sound.
- Fine Tune now begins strictly after the four-band engine.
- Mix blends only the post-band Fine Tune stage, so it no longer weakens the four-band result.
- Density, Body, Detail, Glue, Punch and Space have substantially wider/more audible ranges.
- Glue is the dedicated stereo-linked bus compressor with bus-style attack/release behaviour.
- Punch is independently reactive even when Glue is low.
- GUI positions were recalculated around the actual artwork scale axes.
