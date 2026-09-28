# Reflection

## Vasu

### Learnt

Use of Enhanced Input Component Systems for various player inputs - walk/run/crouch

Custom Interactable class for general interact that can be inherited and overridden for specific uses e.g picking up candle/climbing/opening doors.

Use of state trees instead of the older behavior trees with blackboards - this is because state trees are more memory efficient using an event-driven execution model instead of constantly re-evaluating every tick as well as having cleaner data binding.
Custom Patrol Points that can be dropped into the map containing array pointed to next location.
Building custom state tree tasks for a specific purpose like getting a patrol point, setting a state and triggering capture.
Use of EQS for more accurate investigate as point is picked off of the best possible spot, with multiple searches.

Use of Stimulus perception and deactivating/activating when necessary

Dedicated GameMode .cpp for handling player capture and resetting states.

Candle component code built from the ground up with its states - lit/out/cupped with complexity in mind; pickup, relighting with various conditions, use of a custom noise subsystem built by Ty.
Building of a custom draft volume that can be placed anywhere for the purposes of blowing out the candle when in the draft, keeping design in mind by blowing after guttering for a second.

A chair escape task, modelled after a torsonic damped harmonic oscillator (had to learn the math before code) for realism.

Use of delegate events across the project so that ty can drop in the animation whenever to smoothly update it such as when candle gutters or when player has escaped from the chair.

### Difficulties

Scope creep - I would add a bunch of features, and keep adding more to make the quality better and end up spending way too much time perfecting one thing.
I found that when adding one thing to the game, it breaks other things as well and had to go back to refine stuff because it wasn't initially planned out well.
Spent way too much time debugging stuff - sometimes ending up taking majority of my work time for the whole day.

### Could be better

Time management
Keep building up on the code instead of trying to finish it out perfecting one thing at a time.

## Ty

### Learnt

Building a noise event system as a world subsystem. Footsteps, anim notifies and the candle all report noise events with a loudness, and the subsystem passes them to the AI through the hearing sense so the Keeper reacts to sound the same way no matter what made it.

Noise occlusion using line traces through the level geometry, so a sound behind a wall or a closed door carries less than one in the open. This is what makes hiding in another room actually work.

Creature animation hooks that listen to the AI state and the candle and chair delegates Vasu set up, so the right clip plays for patrol, investigate, chase and capture without the AI code knowing about animation.

Surface traversal for the Keeper: climbing walls, crossing ceilings and squeezing through doorways and child sized gaps. I learnt that Unreal's sweep only tests the start and end rotation, so turning onto a wall clipped through geometry until I tested the in-between rotations myself.

Getting a creature from Blender into Unreal: rigging, exporting clips, morph targets for the mouth and blinks. Unreal ignores the FBX export scale, so it has to be set on import, and materials on a mesh with morph targets need the right usage flags or they render as the default grid.

Building the house greybox in Blender and bringing it in, then a first art pass: textures, boarded windows, turning off the work lights and using fog, vignette and grain so the only real light is the candle.

Using Python editor scripting to import assets and to write automated play tests that run the game and check things like the house being sealed and the Keeper being able to reach every patrol point.

### Difficulties

The asset pipeline took far more time than the gameplay code. Scale, skeletons, re-imports keeping old settings and missing material flags each cost me hours for things that looked small.

I rebuilt the Keeper model more than once chasing a better version and we ended up back on the original because it was scarier. That time would have been better spent on the game itself.

Automated tests passing did not mean the game felt right. The editor world had no collision on the upper floors so tests reported holes that were not there, and the Keeper passed checks while being tiny in game.

Working in the same files as Vasu, like the player character and AI controller, caused merge conflicts, especially after squash merges.

Running Unreal on Linux had its own problems, like crashes from the Wayland backend and the GPU running out of memory with two editors open.

### Could be better

Lock the creature design early and stop reworking it.
Play the actual game sooner and more often instead of trusting the tests.
Smaller merge requests more often, and agree on who owns shared files before we both change them.

### AI use

AI was used to help teach concepts and to work through programming bugs where my knowledge was lacking.
