# CGRA252 (2026) — Individual Project Reflection
Name: Vasudev Menon
Username: menonvasu
Project Role: Core Mechanics, AI Systems & StateTree, Player Controller, Interaction & Minigame Framework
Animal Role: 🐺 Wolf
Project: Tallow (Team 9) | Unreal Engine 5.8
Project Repository Link: https://gitlab.ecs.vuw.ac.nz/course-work/cgra252/2026/project1/team9/unreal_project

## Group Contribution
1. Contribution to the Project
Our team split responsibilities along the Systems vs. Technical Art/Audio boundary:
My Scope (The Systems): I owned the underlying gameplay systems: the physical escape (ATallowChairEscape), the AI State Machine and Environment Queries (ST_AIBehaviour, EQS_SearchArea, EQS_FindHidingSpots), the player interaction and hold framework (UTallowInteractionComponent, IInteractable), the item data-driven architecture (UTallowItemDefinition), the 2D pin lockpicking simulation (UTallowLockpickGame), the barricaded door kinematics (ATallowDoor, ATallowPlank), the projector, film reel and escape system (ATallowProjector, ATallowFilmReelPickup, ATallowEscapePoint), the player class, candle state machine (UTallowCandleComponent and UTallowCandleComponent) and the core soft-reset lifecycle hooks in ATallowGameMode.
Tyron’s Scope (Presentation & Environment): Tyron handled 3D environment architecture, the 3-floor house blockout, lighting, Metasounds, character animation clips, the Keeper rig, and the cinematic director (ATallowPlayDirector). He also did the balancing and tying everything together with better AI, UI, story beats and flow.
The Interface: We designed our C++ classes so Tyron’s presentation layer acted as an observer. My systems exposed delegates (OnPowerRestored, OnPlankDestroyed, OnLockpickSet, OnCaught), and Tyron's presentation actors subscribed to them without modifying my underlying game logic.
2. Communication Role & Fulfillment
We communicated through GitLab Merge Requests, issue trackers, and Discord frequently to update each other.
Code Review Protocol: Every major feature was merged through reviewed GitLab Merge Requests (e.g., MR !11 for AI refinement, the M3 Greybox handoff, and the M4 Task Integration).

## Code Discussion
1. Overview of Code Involvement 
    - Interaction Framework (IInteractable, UTallowInteractionComponent) - All	
    - Lockpicking Simulation (UTallowLockpickGame, UTallowLockpickWidget) - All
    - Barricaded Door & Planks (ATallowDoor, ATallowPlank) - All	
    - Fuse Box & Pickups (ATallowFuseBox, ATallowFusePickup, UTallowItemDefinition) - All
    - Opening Chair Task (ATallowChairEscape) - All	
    - Drafts & Hiding Integration (ATallowDraftVolume, ATallowHideVolume) - All
    - AI & AI State Tree (ST_AIBehaviour) - Half (Refined by Ty)
    - Player and Candle component - Most
    - GameMode Soft-Reset (ATallowGameMode)	- Some (Modified and improved significantly by Ty)
    Overall Project Contribution - Half, Balanced 50/50 split across the entire project

2. Video Walkthrough Link
- Video Link: https://drive.google.com/file/d/11ptylbKTUhZOF7GmVfAZgLk-nMUKjtbA/view?usp=sharing

3. Most Interesting Code: Dual-Stage Ground-Trace Drop with Normal Filtering
File Reference: Source/Tallow/Player/TallowCharacter.cpp#L455-L519
Technical Context & Implementation:
In first-person games, dropping items into a physical world is an annoying bug vector: spawning items with dynamic physics often causes them to clip through stairs, fall into the void, or explode outward when spawned inside geometry.
To solve this, I designed a ground-trace algorithm using surface normal slope filtering.
### Why it is interesting:
Instead of forcing the tool to always drop to the player's shoes, it first evaluates the surface the crosshair is looking at. By filtering against ImpactNormal.Z >= 0.7f, the dot product between the surface normal and the world up-vector makes sure that the surface is a horizontal plane
If the player looks at a desk or chair seat, it detects that, and places the tool flat on that surface.
If the player looks at a vertical wall or door, it rejects the hit and immediately falls back to cast straight down along z to land the tool on the floorboards at the player's feet.

4. Code I Am Most Proud Of: The 2D Pin Lockpicking Simulation
File Reference: Source/Tallow/Tasks/TallowLockpickGame.cpp
### Why I Am Proud of It:
Rather than implementing a generic Skyrim radial dial, I built a mechanical simulation of a real-world pin lock using a 2D overlay. The realism makes the system complex and difficult to solve and gives the player a challenge. So much so that during playtesting people were unable to figure it out even with prompts and adjustments had to be made to make it simpler. It was also really enjoyable to create the visual stress cues such as the shivering of the pick and the increased friction on the binding pin. 


5. Bad Code / Refactored Technical Debt
The "God class" anti-pattern in ATallowCharacter. Earlier in the semester the character class had ended up becoming massive and bloated. It owned interaction, timers, camera and widget.
### Why it was bad:
It violated the Single Responsibility Principle. If the player was cupping a candle or tied to a chair, interaction code was still cluttering the character header. It made merging with Tyron's animation code prone to conflicts and prevented us from reusing interaction logic on other pawns. It was mainly an issue of the interaction becoming too big of a system to keep within the character class. 

6. Review of AI use
Throughout this prjoect, I used LLM's like Gemini as an architecture lead and code auditor.
### How I used it:
Every time I wrote a complex system, I submitted my code for a strict quality audit. The AI was directed to point out memory leaks, dangling pointers and cache inefficiencies. The AI would also point out architectural flaws in my design, explain and provide information to newer engine or language features that I could be using to improve code quality. This helped me improve performance in whatever I wrote, make code more readable and allowed me to experiment with features I was not aware of. A small example of this would be the use of TWeakObjectPtr to hold temporary objects for better performance instead of using standard (*) pointers.
### What I learnt from it:
AI is an incredible tool for understanding why the compiler or engine behaves a certain way, but it is dangerous if used blindly. Early on, an LLM might suggest to "just do a Lerp on Tick" or "put a raycast inside the widget"—suggestions that violate Separation of Concerns and destroy CPU cache locality. By forcing the AI to act as a strict reviewer enforcing RAII, memory alignment, and zero-tick constraints, I learned how to debug the engine and put performance first.

## Learning Reflection
1. Development as a Programmer
At the start of this course, I approached Unreal Engine from an API-usage perspective: calling functions, using convenience libraries, and relying on Tick.
Through building Tallow, my mental model shifted entirely to systems-level execution:
- Cache Locality & Memory Layout: I learned to structure structs like FPinChamber and component data around memory alignment and cache lines, eliminating unnecessary heap allocations in inner loops.
- Zero-Tick Architecture: I learned how to keep complex actors quiet (bCanEverTick = false) by utilizing event-driven callbacks, state-driven delegates, and lower frequency FTimerManager calls (e.g. 10Hz hover checks vs 144Hz frame ticks).
- Decoupled State Machines: Moving from loose boolean flags (bHasFuse, bHasCrowbar) to strongly typed scoped enums (ECarriedItemType) and StateTree HFSMs.

2. The Most Important Takeaway for Future Projects
The most critical engineering principle I will carry forward is the Single Source of Truth / Data-Driven Architecture.
Early in development, I hardcoded class references (CrowbarPickupClass, FusePickupClass) and loose mesh variables across the player and pickups. This created weak dependencies where modifying an asset in one place broke visuals in another.
The difference between this game and the game I had made for my first assignment is night and day. I barely used any classes and had the "God class" problem. I've also become familiar with Unreal's Reflection System and its own style of C++, and how to read documentation straight from within the editor whenever I needed to find specific uses for the problem I was trying to solve. Because of this, I was able figure out better ways of solving the same problem, while also making my systems impressive and complex such as the use of FStateTreeTask and FStateTreeEvaluator.