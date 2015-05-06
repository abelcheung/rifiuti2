/*
 * GNU .indent.pro file for Rifiuti2
 * Sources still need some manual edit to achieve desired
 * result, where tabs of any size would still look good.
 * - Some continuation lines have tabs partially replaced by spaces.
 * - Code after switch/case are indented by 2 spaces instead of a tab.
 */

-gnu
-bfda
-bli0
-bls
-cbi2
-cli2
-cs
-di16
-hnl
-i4
-l80
-psl
-ts4
-ut

/* Declarations */
-T FILE
-T rbin_type
-T rbin_struct
-T uint32_t
-T uint64_t
-T int64_t
-T off_t

/* Glib types */
-T GStatBuf
-T GSList
-T gboolean
-T GLogLevelFlags
-T gpointer
