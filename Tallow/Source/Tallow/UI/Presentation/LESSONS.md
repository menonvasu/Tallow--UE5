# Contextual lesson integration

UTallowLessonWidget is a standalone presentation widget. No player, controller, map, gameplay Blueprint or lockpick code is modified. Lockpicking remains Vasu's #17 work.

## Owner and lifetime

Create one widget per run and retain it in a strong UPROPERTY on a controller or other owner surviving pawn capture. Add it to the viewport; it paints no panel until ShowLesson succeeds. It never grabs keyboard focus, consumes input, pauses play, starts timers or changes gameplay.

The completed/seen sets live on the widget instance. Do not recreate that instance or call ResetLessons on capture/death. DismissLesson may hide an interrupted prompt without erasing completion. Remove/re-add the same retained instance for menus as needed. A level reload that destroys the owner needs an explicit handoff to persistent ownership before use; this class does not pretend to save progress across destruction or application launches. Call ResetLessons for an explicit fresh run.

## Gameplay-owned calls

- ShowLesson(Chair) at the safe opening; complete on successful chair escape.
- ShowLesson(CandlePickup) near the pickup; complete on acquisition.
- ShowLesson(Draft) at the teaching draft; caller completes after the designed observation trigger.
- ShowLesson(Cup) while wind is demonstrated; complete on successful protected state.
- ShowLesson(Relight) with flame out; complete on successful relighting, not the button press.
- ShowLesson(Stillness) in the safe teaching area; complete after gameplay's required hold succeeds.

These are suggested connection points, not implemented triggers. OnLessonCompleted broadcasts the completed enum exactly when CompleteLesson has an active prompt. It clears the active prompt before broadcasting, so a listener can immediately ShowLesson for the next step. There is no implicit sequence or automatic advance. Replaced or dismissed lessons remain incomplete; completed lessons cannot replace the current prompt.

Use CompleteLessonIfActive(ExpectedLesson) from gameplay callbacks; it rejects results belonging to an earlier prompt and returns whether it completed the expected lesson. CompleteLesson remains a convenience for synchronous UI/controller code that intentionally completes the currently displayed prompt.

Stillness copy describes avoiding sight detection, not extinguishing or visually hiding the flame. Current player C++ unregisters sight stimulus and blocks movement/look while held; it does not switch off CandleLight.

## Controls and rendering

Caller-editable FText fields RockKey, InteractKey, CupKey, RelightKey and StillnessKey should be populated from the active input mapping before showing lessons and updated after remapping. Defaults match README controls and C++ action semantics: Space, E, RMB (hold), R (when out), F (hold). This widget does not inspect input mapping assets or change keybindings.

Native Slate paints one compact bottom-centred panel, maximum width 640 Slate units, ivory/ochre wrapping text on a dark inset. It uses viewport DPI scaling. Hide it when the pause/menu overlay is open; do not stack it atop the chair meter if their screen positions overlap. No instructional wall, keyboard capture, fade timer or full-screen dark overlay.

## Verification

Automation: Tallow.Presentation.Lessons.Lifecycle covers invalid lessons, seen versus completed, prompt replacement, dismissal/reoffer, completion suppression, capture-style dismissal, explicit reset and remapped label text.

Root integrator must compile and run the test. No engine build has been run in this worktree to avoid concurrent builds. Then verify viewport rendering at 1280x720 and 1920x1080, long remapped labels, removal/re-addition, menu suppression and capture without repeat of completed lessons.
