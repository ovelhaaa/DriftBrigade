# Frozen M2.2 differential oracle

Copied from commit 9bf785e28542be8511fdcf3e9e86ecf78c8f1ad5 before optimization.
The original filenames are retained so the frozen core includes its frozen
filter and frozen character implementation, never the optimized headers.

Only integration changes: `drift` becomes `drift_reference`, DspMath's include
path resolves from this directory, and read-only state snapshot methods are
available under DRIFT_BBD_INSTRUMENT. Equations, scheduling, pole tables,
character operations and RNG order are unchanged. Do not update this oracle
when changing the optimized implementation.

These files are compiled only in drift_bbd_performance_qualification. They are
never linked into drift_dsp or the plugin.
