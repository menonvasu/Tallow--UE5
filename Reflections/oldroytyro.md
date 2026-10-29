# CGRA252 (2026) Individual Project Reflection
Name: Tyron Oldroyd
Username: oldroytyro
Project Role: Art Direction, Level & Space Design, Mim (Creature Design, Animation & Hunter AI), Audio, Noise Propagation, Lighting, Chapter Flow & Presentation
Animal Role: 🐻 Bear
Project: Tallow (Team 9) | Unreal Engine 5.8
Project Repository Link: https://gitlab.ecs.vuw.ac.nz/course-work/cgra252/2026/project1/team9/unreal_project

## Group Contribution
1. Contribution to the Project
We split the project along the line between systems and presentation, art and audio.
My Scope: the house (the three floor blockout at child scale in Blender, then the interiors, materials and lighting), Mim (her design, model, rig, face and mouth, animation and voice lines), Mim's hunter AI and movement built on top of Vasu's AI base, the noise event system with occlusion, all of the audio and music, the chapter loop and its story beats, the menu, the jumpscare, the story slides and the interface, and balancing the whole game so it plays as one thing.
Vasu's Scope: the core mechanics. The chair escape, the base AI controller and StateTree with its EQS queries, the interaction framework, the item data assets, the lockpick, the barricaded door and planks, the projector, film reel and escape, the player class, the candle state machine and the soft reset in the game mode.
The Interface: Vasu's systems fire delegates (power restored, plank destroyed, lock set, caught) and my presentation, animation and audio listen to them without changing his logic. That let us work at the same time without stepping on each other most of the time, and it is why his systems could become the pieces each chapter is built from.

2. Communication Role & Fulfillment
We talked on Discord most days and kept work on GitLab issues and merge requests, reviewing each other's merges into main.
Where we did share files (the player character and the AI controller) we hit merge conflicts, especially after squash merges, and I learnt to agree on who owns a shared file before either of us changes it.

## Code Discussion
1. Overview of Code Involvement
    - Noise Event System & Occlusion (UTallowNoiseSubsystem, UTallowNoiseOccluderComponent) - All
    - Mim's Hunter AI: brain, director and tactics (ATallowHunterAIController, UTallowHunterDirector, stalk, spider, corner and shadow route tactics, recovery planner) - All, built on Vasu's base
    - AI base & StateTree (ST_AIBehaviour) - Half (Vasu's base, refined by me)
    - Mim's Movement & Traversal (UTallowHunterMotionComponent, UTallowHunterTraversalComponent, UTallowMimMovementContract) - All
    - Creature Animation (UTallowMimAnimInstance) - All
    - Chapter Loop & Beats (UTallowChapterSubsystem, UTallowBeatPresenter, UTallowSaveGame) - All
    - Presentation: menu, jumpscare, story slides, interface (ATallowPlayDirector and the UI widgets) - All
    - Audio (UTallowKeeperVoiceComponent, UTallowHouseAtmosphereComponent, footsteps and soundscape) - All
    - Player presentation: camera, walk sway, breath feedback (UTallowBreathFeedbackComponent) - Some
    - GameMode Soft-Reset (ATallowGameMode) - Some (Vasu's, modified by me for the chapter loop)
    - Editor tools: Python scripts for importing, placing and checking the house and Mim - All
    Overall Project Contribution - Half, Balanced 50/50 split across the entire project

2. Video Walkthrough Link
- Video Link: https://drive.google.com/file/d/1q-bnYSas2YvrSVuQepitIWeltOKU-f1k/view?usp=sharing

3. Most Interesting Code: Noise Occlusion Through the House
File Reference: Tallow/Source/Tallow/Noise/TallowNoiseSubsystem.cpp#L173-L256
Technical Context & Implementation:
Every footstep, dropped item, door and candle reports a noise to one world subsystem, which passes it to the AI through Unreal's hearing sense. On its own the hearing sense hears straight through walls, so hiding in the next room did nothing.
TraceOcclusion walks a line from the noise to the listener and multiplies the sound down by every surface it passes through. Each wall takes away its own share, and a door's share changes when it is opened or closed.
### Why it is interesting:
The trace has to ignore the right things or the numbers come out wrong. Characters don't block sound. A noise made on the floor or against a wall starts touching that surface, so hits in the first few centimetres are skipped. An open door lets all sound through, so it is ignored from then on. The back face of a wall is the same wall, so a second hit on the same component close to the first isn't counted twice. The result scales both the loudness and the range handed to the hearing sense, which is what makes a closed door protect you and makes creeping in the next room actually work.

4. Code I Am Most Proud Of: Mim's Hunter AI
File Reference: Tallow/Source/Tallow/AI/Hunter/TallowHunterStalkTactics.cpp and TallowHunterDirector.h
### Why I Am Proud of It:
The AI was the biggest and hardest part of the project. In early playtests I wrote "I already know he's gonna come to the door every time": the creature was predictable, so it wasn't scary. I rebuilt it around two brains, the way Alien: Isolation does it. The director shapes the evening into long quiet stretches, a build where Mim drifts closer and is glimpsed passing, a peak of real encounters, then a breather after a chase or a near miss. It decides what she is allowed to be (faster, sharper eared, quicker to come back), but it never tells her where Nell is. She has to find Nell through what she senses: a lit candle, a sound, or breathing close by. A still child holding her breath in the dark is never found, even from the ceiling. That rule is what keeps the game fair.
On top of that she has her own ways. While the evening builds she stalks: she stands off in the dark a room or a hall away, somewhere Nell can see her, holds still and watches. Turn the light on her and she slips out of sight, then comes back a little closer the next time, far, nearer, then close. She rests as a harmless looking heap that unfolds when you get near, hangs from the ceiling over a doorway and drops, waits at the mouth of a crawl or on top of a cupboard, and uses the same crawl links as the player to vanish and come out somewhere else. Being seen watching you is scarier than being chased, and it is the part of the game I am happiest with.

5. Bad Code / Refactored Technical Debt
Mim's movement had several layers that could each veto a move on their own: posture, fit to the space, floor support and the grips for her hands each ran their own collision checks.
### Why it was bad:
Each layer used different tolerances from the real capsule physics, so they disagreed with what was actually happening. She froze halfway through a slide, stalled in doorways, and every new fix added another check that could veto her again. I refactored it into one movement contract (Tallow/Source/Tallow/AI/Hunter/TallowMimMovementContract.h#L47-L66). Character movement and the navmesh are now the only physical authority. The contract only measures: if the capsule is blocked she folds into a compact posture, and she can only grow back to full size if the move is fully valid, while shrinking is always allowed because smaller can never fit worse. When she does get stuck, a separate recovery planner finds a way out.
Some of the debt is still there. Her corner climb looked right, but a 17 minute soak test measured every one of its 14 climbs with her arms or body 27 to 38 cm inside the walls, so it stays switched off until the climb itself is clean.

6. Review of AI use
Throughout this project, I used Claude to help me build the architecture of my systems and to work through bugs.
### How I used it:
I wrote my systems myself: the noise system and occlusion, Mim's AI, her surface traversal (climbing walls, crossing ceilings, squeezing through gaps) and the animation hooks that follow her AI state. Most of that was written earlier in the project, and I wrote fast to get things working. I used Claude to help turn that code into proper architecture, splitting it into components and subsystems and deciding what talks to what through delegates, and to help me debug. As the systems grew I had to go back, rewrite and fix parts of them, and I used it to help with that rework. It also explained why the engine behaved the way it did for the parts of Unreal I didn't know well yet (the hearing sense, StateTree, the navmesh, character movement and shader errors on Vulkan), which helped me understand bugs like the walk sway passing a field of view into AddAdditiveOffset, or why a navmesh saved from a script went stale.
I recorded all of Mim's voice lines myself, but hearing my own voice was making me cringe a bit, so they are dubbed over with ElevenLabs to give her the voice of a young girl. The hidden corrupted narration uses that same voice so it matches her. Some AI 3D generators were used early on for placeholder models, but the final models in the game were all built by me.
### What I learnt from it:
AI is a great tool for understanding problems I don't have the knowledge for yet, but it is dangerous if used blindly. A suggested fix could pass the automated tests while the game still felt wrong, and when a bug came back it tended to suggest another check on top instead of finding the cause, so I had to go back and find the cause myself. I learnt to use it on code I already understood, to check what it told me against what the engine actually did, to measure and log what my systems were doing (soak tests, decision logs, clipping in centimetres) so I could see why something happened, and to never trust a fix until I had played it in the real build myself.

## Learning Reflection
1. Development as a Programmer
I started this project as an artist who wrote code to hold the art together, and finished it writing systems.
- Event driven systems: the noise subsystem taught me to have everything report to one place, so Mim reacts to sound the same way no matter what made it.
- What you see and what is physical are separate: Mim's body is a huge soft mass of plush and stocking arms, but her capsule stays small for the navmesh and the body squeezes to the free space around it every frame. Clipping only matters when the player can see it, which stopped me chasing problems nobody would notice.
- Measure instead of eyeballing: Mim logs why a search turned down every candidate spot, the director keeps a decision log, and soak tests measure clipping in centimetres. Most of the AI bugs were only fixable once I could see why she made a choice.
- The pipeline is code too: Unreal ignores the FBX export scale, materials on a mesh with morph targets need the right usage flags, a material can compile on one renderer and fail on Vulkan, and saving a map from a script without rebuilding navigation ships an old navmesh where Mim can't get between floors, while the editor rebuilds it on load and hides the problem. I moved map changes into editor scripts that can be run again instead of hand edits.
- Read the signature: the screen went black whenever you walked, because the walk sway passed the field of view as the second argument of AddAdditiveOffset, which is an extra field of view, not the current one.

2. The Most Important Takeaway for Future Projects
Play the real game every day, and get the structure right before polishing it. Tests passing never told me the game felt right. The game used to be one long hunt for every battery, and only playing it showed that Mim being there all the time made her stop being scary. I restructured it into chapters (explore while she is only a plush toy, find a tape recording, the blood writing tells you the goal, she changes, Nell blinks and wakes in the chair, then a real hunt for one battery before the house resets), so every chapter starts quiet and the hunt has something to break. Playing also showed that a mechanic without feedback doesn't exist to the player: I pressed F to hold my breath, froze, and had no idea if it was working. The same goes for the jumpscare, which only landed once the camera turned you to face her straight on and held the stare.
At the prototype I wrote that I should lock the creature design early, and then I redesigned her again. It was right for the game, but the movement and AI are built around the body, so next time the body gets locked before any of that starts.
