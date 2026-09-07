# TALLOW

**Course:** CGRA252 (2026), Project 1
**Group:** Team 9
**Engine:** Unreal Engine 5.8

> Tallow is what candles used to be made of. Rendered fat. Something that was a body, turned into a thing that burns.

---

## Team

| | Name | Username | Role |
|---|---|---|---|
| :bear: | Tyron Oldroyd | `oldroytyro` | Art direction, level and space design, animation, all audio, noise propagation system, lighting and VFX |
| :wolf: | Vasudev Menon | `menonvasu` | Core mechanics, AI behaviour trees, player controller, interaction and minigame framework |

## Design Video

Approximately 2 to 4 minutes, walking through the design.

**[ Watch the TALLOW Design Video ]** ( https://drive.google.com/file/d/1nauq0n2xJdngfQN-RJxJgxfaS6lw4g9e/view?usp=sharing )

---

## Game Description

### Premise

You wake up tied to a chair in the basement of a house that belongs to something that collects children. It does not kill them. It takes them apart and puts them back together as toys, and it keeps them on its shelves.

You are still yourself, for now. Your hands are still your hands.

The house is small, it was built for someone much larger than you, and it is full of the children who came before. To get out you have to work your way up through it, and decide how many of them you are taking with you.

### Style and Comparable Games

A first person horror game built on stop motion and illustration references rather than photoreal horror. Everything in the house looks assembled: carved wood, porcelain, sackcloth, buttons, visible stitching. No realistic skin anywhere in the game.

| Reference | What we take from it |
|---|---|
| **Coraline**, Dave McKean's illustrations | Made faces, button eyes, red thread, sickly green and gold against candle amber |
| **Little Nightmares** | Doll proportions, heads too big and limbs too long, scale as threat, no HUD |
| **Švankmajer and the Quay Brothers** | Puppet materiality, carved mask heads, the seam as the horror |
| **Alien: Isolation** | A pursuer driven by sound, hiding that is tense rather than safe |
| **Amnesia: The Dark Descent** | Consumable light, a flame you have to protect |
| **Granny**, **Hello Neighbor** | Task driven escape from a compact house under a patrolling stalker |

The rule underneath all of it: **everything in this house is a puppet, including you.** The player should work out that the house is a dollhouse about twenty minutes before they work out what they are.

**Palette.** Ochre, grey, black, dust, with one saturated colour carrying all the meaning. That colour is red thread. It marks where a child was unmade, so it works as wayfinding, as story, and as the only warm thing in the frame besides the candle.

**2D elements.** The world is 3D, the child's understanding of it is 2D. Minigame overlays, the lock interior, the projected film, and story beats are flat and hand drawn in a storybook register. The 2D is diegetic, never decorative.

### Main Game Mechanic

Two detection channels that pull against each other. The creature can hear you and it can see your light, and everything you need to do produces both.

**1. The noise economy.**

```
player action
  -> noise event (position, loudness, type)
    -> creature perception check (distance, occlusion)
      -> AI state change: Patrol / Investigate / Chase
        -> player re-plans
```

Every task in the game is a deliberate noise generator. The tension is never the puzzle itself, it is choosing when to make the noise and how far away the creature is when you do.

**2. The candle.**

You carry a candle and light it with matches. Dark means invisible but unable to work. Lit means able to work but findable, and light leaks under doors and across hallways, so you can give yourself away from a room away.

Relighting is a match strike, which is a sharp noise and a bright flash at the same time. If the creature is in the room when your candle goes out, you cannot relight. You have to move away in complete darkness first.

Balance the candle on burn time, not on match count. Being stranded with no matches because of a bad spawn is a rage quit, not a scare.

**What puts the candle out:**

| Cause | When | What it teaches |
|---|---|---|
| Draft from a broken window, a gap under a door, a vent | Always | Map knowledge, certain rooms are windy |
| You ran | Always | Panic costs you your light |
| The creature passing nearby | Phase one | It did not mean to, it moved and the air moved. Your candle gutters and dies and you know it just walked past |
| The creature blowing it out | Phase two only | It knows where you are and it wants you in the dark |

**Cupping the flame.** Hold to bring a hand around the candle. Protected from drafts, but you move slower, the light radius shrinks, and that hand cannot interact with anything.

**Fixed flames.** Pilot lights, fireplaces, a candle on a shelf. Free relight points that cost no match, and they double as the safe pockets in the level. Fear needs rhythm, and the player needs somewhere to breathe before you take it away.

**3. Stillness.**

To survive it you have to be a toy. When it is close and searching, hold to go completely rigid. You cannot move, turn or look. Its detection drops and it passes over you.

You save yourself by pretending to be the exact thing you are afraid of becoming.

Going still accelerates the transformation. Every time you use it, the seams on your hands spread a little further, so the safest action in the game is also the one that dooms you. How much of you is left at the end drives the ending.

### The Creature

Two phases, and the phase is the difficulty curve. Escalation happens on screen rather than as a hidden stat.

| Phase | Behaviour |
|---|---|
| **The Host** | Upright, slow, almost courteous. Patrols, opens doors, investigates sound. Cannot fit through the small gaps. Hiding works. |
| **The Thing** | The host body splits open into a grotesque spider limbed form. Wrong joints, climbs walls and ceilings, follows you into the gaps, blows your candle out on purpose, and defeats hiding places that used to be safe. |

The transformation is staged once, deliberately, at a distance and through a doorway, so that the player actually sees it. In first person it will be missed entirely if it happens while they are running.

Both phases are animated stepped at roughly 12fps against a 60fps world, the way Laika and Švankmajer animate on twos. It costs almost nothing and it makes the creature move wrong in a way players feel without being able to name.

**The flame as a cue that lies.** In phase one the candle leans and gutters when the creature is near, and the player learns to trust it. In phase two that stops being reliable. They were trained to depend on it so that it could be taken away.

### Scale and Traversal

Being a child is a mechanic, not just a feeling.

The player capsule is roughly 120 to 130cm with the camera around 110cm. World scale stays at Unreal default so that marketplace assets and physics keep working, and the specific objects a player knows the true size of are oversized by hand: door handles, light switches, chair seats, counters, stair risers, keyholes, cups. Scale is only perceptible against a known reference, so uniform scaling would be invisible.

The house is arranged the way someone displays a room rather than lives in one. Chairs squared to tables, everything facing front, nothing casually out of place.

Consequences for play:

- You cannot reach handles, so you drag a chair over and climb
- You fit through gaps, vents, cupboard backs and holes in the skirting that the Host cannot use
- Those gaps are routes and refuges at the same time, until phase two takes them

### Hiding

Hiding buys time and uncertainty, never safety. If the creature saw you get in, it comes straight to that spot. If it only heard something nearby, it searches a few.

Every hiding place has a view out: bed slats, wardrobe louvres, a crack in a door. You watch it come into the room, search, and leave.

Your candle has to be out. A wardrobe with light coming through the louvres is the worst hiding place in the house, and the light system enforces that without any extra code.

### Tasks

Each is a distinct interaction and a distinct noise signature.

| Task | Interaction | Noise profile |
|---|---|---|
| **Chair escape**, the opening | Rock back and forth on spacebar timing to break the restraints | Rhythmic, loud on mistimed rocks. Teaches the noise rule before the creature is a threat |
| **Lockpicking** | 2D overlay of the lock interior, feel for the pin | Silent on success, sharp snap on failure |
| **Fuse box** | Insert 2 to 3 fuses to restore power | Loud on every insertion, three guaranteed alerts that the player spaces out |
| **Crowbar and planks** | Hold to pry three boards off a door | Continuous noise while held, so the player chooses when to stop |
| **Film projector** | Find a reel, thread it, read a code off the projected film | Mechanical clatter, and the projection is one of the 2D story beats |

### Challenging Engine Technology

**AI and gameplay systems, led by Vasudev**

- Unreal Behaviour Trees, Blackboards, Decorators and Services for the three state AI
- Environment Query System for search positions, hiding spot checks and phase two ambush points
- AI Perception with both hearing and sight, and custom stimuli sources for noise events and for light
- NavMesh with dynamic modifiers for doors and blocked routes, plus off mesh links for the phase two climbing form
- Interaction and minigame framework, and the candle state machine

**Audio, animation and rendering, led by Tyron**

- A custom noise propagation system: noise events with distance falloff and geometry occlusion, feeding the AI perception layer
- MetaSounds for procedural creature foley and a reactive ambient bed
- Audio occlusion and submix routing
- Animation Blueprints, retargeting, blendspaces and IK for both creature phases, animated on twos
- The phase transition, skeletal mesh swap with morph targets and Niagara
- First person hands that change material and topology across the run, from skin to seams to porcelain
- Candle lighting and VFX, flicker curves, guttering, and light leak through gaps
- Stylised rendering, custom post process for the illustrated look, 2D elements composited into a 3D scene

---

## Scope

### MVP

- One compact house: basement, ground floor, one upper space
- One creature with Patrol, Investigate and Chase
- The noise event system driving AI perception
- Candle, matches, cupping, and the draft and running extinguish causes
- Chair escape plus three further tasks
- Hiding places with a view out, and gaps the creature cannot follow you into
- Stillness as a detection modifier
- Both creature phases and the staged transformation
- One escape ending

### Stretch

- The creature extinguishing the candle deliberately
- Transformation cost driving multiple endings and a survivor count
- Recovering the other children as toys, and wind up toys as noise decoys
- Randomised item and task placement between runs
- Per run AI difficulty scaling
- Fifth task, pressure pad or wire cutters
- Route prediction and interception for phase two

---

## Milestones

| Milestone | Date | Content |
|---|---|---|
| **M1, Design and Pipeline** | 7 Sep 2026 | This README, issue tracker, labels, milestones, branching workflow, design video |
| **M2, Vertical Slice** | 18 Sep 2026 | Greybox house at child scale, player movement, candle, one working task, AI navigating and reacting to a noise event |
| **M3, Prototype** | 28 Sep 2026 | All three AI states, noise system complete, three or more tasks, hiding, stillness, phase one creature, reflection |
| **M4, Final Submission** | 27 Oct 2026 | Full art pass, both phases and the transformation, audio complete, release build and tag |

---

## Development Workflow

Branch off `main`, one branch per issue, merge back through a reviewed merge request.

```bash
# 1. sync
git checkout main
git pull origin main

# 2. branch, named for the issue
git checkout -b feature/4-crowbar-logic

# 3. build in UE5 and save

# 4. commit, referencing the issue number
git add .
git commit -m "feat: crowbar interaction logic (#4)"

# 5. push
git push origin feature/4-crowbar-logic

# 6. open an MR in GitLab, add "Closes #4", assign the other member to review
```

**Branch naming:** `feature/<issue>-<slug>`, `fix/<issue>-<slug>`, `art/<issue>-<slug>`
**Commits:** conventional prefixes (`feat:`, `fix:`, `art:`, `audio:`, `docs:`) and always reference the issue with `#n`.

### Repository Setup

Unreal build output is ignored: `Binaries/`, `Build/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`. Binary assets go through Git LFS, covering `.uasset`, `.umap`, `.fbx`, `.blend`, and all image and audio formats.

```bash
git lfs install
git clone git@gitlab.ecs.vuw.ac.nz:course-work/cgra252/2026/project1/team9/unreal_project.git
```

`.uasset` and `.umap` are binary and cannot be merged. Claim a map or blueprint in its issue before editing it, so that two people are never in the same file.

---

## Assets and Templates

Maintained as assets are added.

| Asset | Source | Licence | Used for |
|---|---|---|---|
| To be added | | | |
