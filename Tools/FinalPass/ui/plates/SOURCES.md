# Story plates and wordmark: sources and licences

Build: `cd ~/Projects/tallow-current/Tools/FinalPass/ui/plates && python3 plates.py && python3 wordmark.py && python3 sheet.py`
(`python3 plates.py Intro_03 EndB_01` rebuilds only the named plates.)

Everything in `out/` is drawn by these scripts (numpy, pycairo, Pillow). Paper, ink, pencil and wax
crayon are procedural; every word, stroke and object is placed by hand in `plates.py` and `drawing.py`.
No photographs, scans, traced images or generated imagery. Each PNG carries a `provenance` text chunk.

## Fonts (copies and licence texts in `fonts/`)

| Use | Face | Designer | Licence |
|---|---|---|---|
| Newspaper body, ledger column heads | IM FELL English Regular and Italic | Igino Marini | SIL OFL 1.1 |
| Newspaper dateline | IM FELL English SC | Igino Marini | SIL OFL 1.1 |
| Newspaper masthead, title wordmark | IM FELL French Canon | Igino Marini | SIL OFL 1.1 |
| Newspaper headlines | Old Standard TT Bold | Alexey Kryukov | SIL OFL 1.1 |
| Police slip (typewriter) | Special Elite | Astigmatic | Apache 2.0 |
| Pryce's register, the candle label (the careful hand) | Pinyon Script | Nicole Fally | SIL OFL 1.1 |
| The constable's letter, 1894 | Euphoria Script | Sabrina Lopez (Typesenses) | SIL OFL 1.1 |
| Maisie's hand | Reenie Beanie | James Grieshaber | SIL OFL 1.1 |
| Nell's hand (the numbers on her drawing) | Shadows Into Light Two | Kimberly Geswein | SIL OFL 1.1 |

Why these hands: Pinyon Script is an engraver's copperplate, a clerk's roundhand that does not change
across a century, which is the point of the register ("the same careful handwriting"). Euphoria Script
is a plain, slightly hurried running hand, so the constable's letter reads as an ordinary adult and
never as the register's hand, and the name Elsie stays legible at a glance. Maisie's hand is scrawled and fast; Nell's is thin and
careful, as she draws.

All fonts are from github.com/google/fonts. The OFL permits embedding in rendered images; the fonts
themselves are not shipped in the game by these plates.
