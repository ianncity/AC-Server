# Human-like navigation research

Research date: 2026-10-17. Scope: practical, rule-based movement for Playerbot ADVAI.
This document describes findings and proposed implementation, not completed movement features.

## Findings from primary sources

1. **Route realism is a separate objective from shortest distance.** Hanold and
   Petty's *Enhancing a Commercial Game Engine to Support Research on Route Realism
   for Synthetic Human Characters* distinguishes route efficiency from similarity
   to human routes under the same goals and constraints. It instruments position,
   heading, time and velocity and describes a measurement methodology. Its
   contribution is instrumentation and assessment methodology; it does not establish
   a finished controller that makes game bots indistinguishable from players.
   Source: https://doi.org/10.1155/2011/819746

2. **Coherent human movement sequences can improve navigation believability.**
   Karpov, Schrum and Miikkulainen's *Believable Bot Navigation via Playback of Human
   Traces* uses recorded human trajectories in Unreal Tournament. Their controller
   selects relevant trace segments for navigation, exploration and recovery.
   Playback needs to respect proximity, continuity and interruptions. The paper
   explicitly discusses the stalling that happens when movement destinations are
   issued too late, and scheduling upcoming destinations to keep movement flowing.
   This is evidence for coherent sequences rather than independent random offsets.
   It is an FPS study, not evidence that its parameters transfer directly to WoW.
   Source: https://nn.cs.utexas.edu/downloads/papers/karpov.believablebots12.pdf

3. **Successful navigation does not itself establish human likeness.** Colbert and
   Saeedi's *Evaluating Navigation Behavior of Agents in Games using Non-Parametric
   Statistics* compares distributions of player and agent movement patterns and
   reports alignment with anonymous human judgments. This supports comparing
   observed trajectories as well as task success. A similarity statistic alone is
   not proof of indistinguishability, and no WoW-specific calibration is provided.
   Source: https://doi.org/10.1109/CoG51982.2022.9893600

## Application to this checkout

The following are engineering recommendations inferred from those findings and
the local implementation, rather than results directly established by the papers.

### First priority: consistent route following and turning

Keep native pathfinding as the source of traversable routes. Add a small movement
controller that selects a point ahead within the validated path corridor, starts
turns with enough notice, and continues toward the next segment without stopping
at each waypoint. Reuse the current route instead of clearing and regenerating it
for every small correction.

Model movement styles appropriate to WoW controls. Some players make keyboard
turns, some make sharp mouse turns, and some hold autorun through long stretches.
Use persistent per-character tendencies with bounded variation. Uniformly smooth
turns or continuously changing speed should not be assumed to look like players.
Use normal native running, walking, mounted and swimming speeds.

### Second priority: purposeful route and approach variation

Choose among a small number of safe, reasonably efficient routes using stable
preferences for known roads, clearance, observed danger and previous route
failures. Keep a route long enough that the bot does not oscillate between nearly
equal choices. Any lateral offset must remain inside a navigable corridor; avoid
adding independent coordinate noise to path points.

Choose different valid approach positions around vendors, quest givers and friendly
players, respecting interaction distance and line of sight. This addresses the
visible clustering that happens when many bots aim for one exact coordinate.
Keep doorways, stairs, docks and other narrow passages precise.

### Third priority: combat locomotion

Separate travel direction from facing where the engine supports it. Add short,
purposeful strafes and positional adjustments based on actual target position,
range, line of sight and visible teammate needs. Preserve essential role behavior.
Retain a movement decision briefly rather than swapping sides every update.
Do not manufacture speed changes or use hidden enemy destinations and cooldowns.

### Fourth priority: interruptions, jumps and recovery

Make pauses follow understandable circumstances: arriving, reading a message,
changing plans or waiting for someone. Keep them uncommon during purposeful
travel and avoid delaying urgent combat responses.

Keep the newly implemented occasional travel hops, but treat them as a personal
habit with appropriate terrain and activity eligibility. Their frequency should
eventually come from examples of players using the same client. Jumping does not
replace route-following work.

When progress stops, cancel the failing attempt, try a bounded local adjustment
or retreat to a valid earlier point, and then replan with a temporary exclusion.
Avoid repeatedly issuing the identical wall-facing move. Preserve explicit blocked
reasons and legitimate gameplay throughout recovery.

## Existing integration points

- `MovementAction::MoveTo` and `DoMovePoint` currently hand movement to native point
  generators. `DoMovePoint` clears motion before issuing a new point movement.
- `NewRpgBaseAction::MoveFarTo` already has destination commitment and progress
  handling. Preserve its protection against repeated movement restarts.
- `SearchForBestPath` calculates candidate paths; the normal `MoveTo` branch then
  hands the selected endpoint to `DoMovePoint` for native path generation. Reusing
  a validated corridor needs care here to avoid duplicating path searches.
- Native `MoveSplineInit` supports facing, walking and orientation controls. This
  supplies building blocks; it does not establish that every desired player input
  pattern is already represented correctly by the current bot movement pipeline.
- `AdvaiMannerisms` supplies occasional native travel jumps and route resumption.
  Existing persona formation offsets and reaction timing provide a starting point
  for consistent styles, but are not a complete movement controller.

## Practical calibration

Record a small set of representative human trips and fights in this client:
position, facing, movement flags, time, jumps and activity context. Separate walking,
autorun, mounted travel, melee, ranged and healer movement before comparing them.
Derive compact rule parameters or short reusable movement patterns from those
examples; a neural model is not required to use recorded examples as a reference.

Useful comparisons include heading changes, waypoint pauses, path length relative
to a viable baseline, turn frequency, jump spacing, approach positions, repeated
stuck attempts, and continuity after interruption. Visual review should accompany
these measurements because matching a distribution can still hide poor behavior.

For the population target, cache routes and movement profiles. Run expensive path
searches when destinations or traversability change. Keep ordinary steering local,
bounded and free of synchronous database access. Actual performance remains to be
measured after implementation; this research does not establish a cost estimate.

Recommended implementation order: continuous route following and turning, varied
valid approaches, stable route preferences, combat strafing, then refinements to
habits and recovery. No build or runtime test was run for this research request.

## Implementation follow-through

The subsequent implementation request adds `AdvaiMovement.*` and integrates it
with native movement actions, RPG travel, life-service approaches, tactical movement
and the existing travel hops. It implements continuous corridors, validated corner
rounding, stable turn styles, familiar routes, observed-threat detours, arrival
spacing, contextual reading pauses and bounded recovery. A PvP action adds short
target-facing sidesteps, pressured retreats and incremental rear positioning.

Road-specific preferences need actual road classification data; the implementation
uses known traversable corridors and observed conditions rather than claiming it
knows where roads are. Parameters remain initial engineering choices. Native builds,
visual acceptance, human trace calibration and population cost measurements remain
deferred; these source changes do not establish measured human likeness.
