# Stadium Dimension Validation — L_Match

Generated via Unreal MCP against the existing `CricketStadium` actor in
`Content/Maps/L_Match.umap`. All comparisons are against ICC playing
conditions and the MCC Laws of Cricket.

| Element | Built value | Real-world reference | Status |
|---|---|---|---|
| Square boundary | 75.0 m (`SquareBoundaryM`) | ICC min 59 m; large grounds (MCG, Eden Gardens) run 70–75 m | Matches request, within real range |
| Straight boundary | 80.0 m (`StraightBoundaryM`) | ICC has no fixed max; large grounds commonly 75–90 m down the ground | Matches request, within real range |
| Pitch length | 20.12 m (22 yards, stump-to-stump) | MCC Law 6 — exactly 22 yards | Exact match |
| Pitch width | 3.05 m (10 ft) | MCC Law 6 — exactly 10 ft | Exact match |
| 30-yard circle | 27.43 m radius (debug-drawn at runtime) | ICC fielding-restriction circle — exactly 30 yards | Exact match |
| Fielding markers | 17 named positions (WicketKeeper through Deep Square Leg), rescaled to sit at the same proportional depth (28–98 m) relative to the new boundary | Standard named cricket fielding positions | Matches convention |
| Stands | Single ring actor (32 segments), outer radius ~116 m, height up to 18 m | Typical tiered stand depth 30–50 m beyond the rope | Clears boundary with 36–41 m of concourse/advertising margin |
| Floodlights | 6 towers, 130 m radius, 60° spacing, 45 m pole height | Major-stadium towers typically 40–75 m (e.g. MCG ~74 m, most international grounds 45–55 m) | Within real range |
| Sight screens | 2 screens, 11 m wide x 6 m tall x 0.3 m thick, placed 4 m inside the straight boundary at both ends | Typical portable sight screens are ~9–12 m wide x 4–6 m tall | Within real range |
| Replay/broadcast cameras | 10 cameras (6 broadcast incl. elevated crane cam at 60 m, 4 replay anchors at stump height) | Standard broadcast rig: stump cams, square cams, elevated wide shot | Matches convention |
| Outfield ground | 95 m x 95 m flat plate | Must clear the largest boundary axis | Clears 80 m straight boundary by 15 m |

## Changes made this session

The stadium previously existed with a 75 m straight / 68 m square boundary
(the inverse of, and smaller than, what was requested). To meet the
75 m square / 80 m straight requirement:

1. `CricketStadium_0.SquareBoundaryM` 68→75, `StraightBoundaryM` 75→80
   (drives in-game six/four detection).
2. `BoundaryRope` (96 rope segments) rescaled non-uniformly
   (×1.0667 on the straight axis, ×1.1029 on the square axis) so the
   rope retraces the new ellipse exactly.
3. All 17 fielding-position markers and both team spawn points rescaled
   by the same factors to preserve their relative depth.
4. Ground, pitch, stands, and all 10 cameras were verified to already
   clear the new boundary with no changes needed.
5. Added 6 floodlight towers and 2 sight screens, which did not
   previously exist as level actors (only as debug-only / data-only
   references in `CricketStadium.cpp`).
