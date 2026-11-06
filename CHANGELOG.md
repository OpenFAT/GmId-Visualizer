# Changelog

## Unreleased

### Added

- Id/W-driven sizing: a second function family keyed by current density Id/W (A/m) instead of
  gm/Id, on the same tables: `gmid.vgs_jd`, `gmid.gm_id_jd`, `gmid.w_jd`, `gmid.gm_jd`,
  `gmid.gds_jd`, `gmid.gain_jd`, `gmid.ft_jd` (`(dev, jd, l[, id])`) and their `_jd_at`
  variants with |VDS| last. Same names as the gm/Id family with `_jd` before `_at`, same
  argument order, so a docs page switches flow by renaming the call; `gmid.gm_id_jd` is the
  inverse of `gmid.jd`. W in the Id/W flow is `id / jd` directly (`gmid.w_jd`).
- `Lut::point_jd` / `Lut::point_jd_at`: the inverse lookup. VGS by scanning Id/W down from the
  highest VGS (Id/W is monotone in VGS, so a gm/Id dip has one answer per Id/W; the branch ends
  at cutoff or where leakage makes the current rise again), the cell fraction solved in log
  Id/W on the curve the gm/Id lookup walks (exact inverse on grid lengths: 4e-16 on the EKV
  oracle, 1e-16 on the sky130 samples), the Id/W key carried to each L row as `jd·l/l_row`
  between rows (agreement 3.2e-5 on EKV; on sky130 2.6 % nfet, 6 % pfet in gm/Id, within
  1.6 / 6.4 mV in VGS). `Point` gains `gm_id`; `Miss` gains `Jd`.
- README: the two flows, the inverse-lookup method and its accuracy, a 5-transistor OTA input
  pair sized both ways (same W).

### Changed

- `FN_STRIDE` is 32 (was 16): 29 functions. fn_ids are generation-scoped and never stored, so
  docs pages are unaffected; the module's hash changes, so D20 caches are rewritten once.
- A failed simulator run no longer copies the tail of the simulator's output into the log (it
  can quote proprietary PDK model files): the output goes to `.schemify/gmid/work/<dev>.log`
  and the log names that file.
