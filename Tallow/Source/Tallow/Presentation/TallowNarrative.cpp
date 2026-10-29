#include "Presentation/TallowNarrative.h"
namespace TallowNarrative {
TArray<FName> Keys(){return {TEXT("journal"),TEXT("register"),TEXT("chart"),TEXT("bill"),TEXT("newspaper"),TEXT("notebook"),TEXT("nan"),TEXT("drawings"),TEXT("labels"),TEXT("ruth"),TEXT("portrait"),TEXT("letter"),TEXT("maisie")};}
FText Title(FName Key){
if(Key==TEXT("journal")) return FText::FromString(TEXT("Maisie's journal"));
if(Key==TEXT("register")) return FText::FromString(TEXT("The treatment register"));
if(Key==TEXT("chart")) return FText::FromString(TEXT("The height marks"));
if(Key==TEXT("bill")) return FText::FromString(TEXT("A missing child"));
if(Key==TEXT("newspaper")) return FText::FromString(TEXT("The winter cutting"));
if(Key==TEXT("notebook")) return FText::FromString(TEXT("The constable's account"));
if(Key==TEXT("nan")) return FText::FromString(TEXT("Nan's warning"));
if(Key==TEXT("drawings")) return FText::FromString(TEXT("The children's drawings"));
if(Key==TEXT("labels")) return FText::FromString(TEXT("The named candles"));
if(Key==TEXT("ruth")) return FText::FromString(TEXT("Ruth's sewing box"));
if(Key==TEXT("portrait")) return FText::FromString(TEXT("The Pryce photograph"));
if(Key==TEXT("letter")) return FText::FromString(TEXT("The unsent letter"));
if(Key==TEXT("maisie")) return FText::FromString(TEXT("Maisie's last page"));
if(Key==TEXT("notes")) return FText::FromString(TEXT("Nell's notes"));
return FText::GetEmpty(); }
bool Printed(FName Key){
if(Key==TEXT("journal")) return false;
if(Key==TEXT("register")) return true;
if(Key==TEXT("chart")) return false;
if(Key==TEXT("bill")) return true;
if(Key==TEXT("newspaper")) return true;
if(Key==TEXT("notebook")) return true;
if(Key==TEXT("nan")) return false;
if(Key==TEXT("drawings")) return false;
if(Key==TEXT("labels")) return true;
if(Key==TEXT("ruth")) return false;
if(Key==TEXT("portrait")) return true;
if(Key==TEXT("letter")) return false;
if(Key==TEXT("maisie")) return false;
return false; }
TArray<FText> Pages(FName Key){
if(Key==TEXT("journal")) return {FText::FromString(TEXT("MAISIE EVANS. HER BOOK.\nKEEP OUT.\n(that means you Nell)\n(not really)\n\nWinter, 1974.\n\nNell believes me. Nobody else does.")),FText::FromString(TEXT("I went into the old house. A staircase. A ceiling too high to see. Then a pale face and all those hands.\n\nThe first time I ran because I was frightened.\n\nI should have run again.")),FText::FromString(TEXT("if you woke in the chair, rock with it. push when it tips back.\n\nit thinks this is somewhere safe.\n\nthe candle and matches are on the table. take them. nothing electric in this house gives any light.")),FText::FromString(TEXT("IT FOLLOWS THE LIGHT.\n\ncup the flame with your hand (right mouse) and it sees less. R puts it out.\n\nbut then you cant see either.\n\nR strikes a match. a match is LOUD.")),FText::FromString(TEXT("in the dark it LISTENS.\n\nhold your breath (F). stay still. crouching (ctrl) is quieter. running (shift) is loud and can kill the flame.\n\nyou cant hold your breath forever.\n\nif it looks at you, hide. a bed. a wardrobe. somewhere small.")),FText::FromString(TEXT("the cellar door has a lock. feel for the pins. hold tension gently.\n\na pin that stays is good. too much and the pick snaps.\n\nIT HEARS THE SNAP.\n\nmore locked rooms upstairs. a way in can become a way out.")),FText::FromString(TEXT("there is a crowbar somewhere in the house. use it on the workshop boards.\n\ndont pull them all at once if it is close.\n\nit hears each one fall.\n\n[ I ] is what you carry. [ Q ] puts a tool down. the candle has its own hand.")),FText::FromString(TEXT("three fuses. one in the cellar, one in the house, one upstairs.\n\nput them in the switchboard.\n\nthe motor still works. the lamps dont. it releases the attic latch.\n\nDONT expect the lights to come on.")),FText::FromString(TEXT("the records are older than mum. older than nan.\n\nFINISHED doesnt mean safe.\n\nthe rooms come from our drawings. things are big because thats how we remembered them.\n\nit is trying so hard to make a home.")),FText::FromString(TEXT("the shelves are upstairs. every candle has a name.\n\nif you find mine, look for me.\n\nbut if I tell you I dont want to go...\n\nnell, please remember what I wanted before.\n\nM.")),FText::FromString(TEXT("p.s. learn the signs at the edge of your eye, nell. you feel them before you see it.\n\nIT SEES ME. the flame bends to it and goes cold. put the light out and get out of its sight.\n\nOUT OF ITS SIGHT. keep going. it goes to where it saw you last.\n\nITS LOOKING. it caught a glimmer. cup the flame or put it out. dont run.\n\nITS CLOSE. the other side of the wall maybe. hold your breath (F) and wait.\n\nI CAN HEAR IT. it hasnt found you. but it is looking.")),FText::FromString(TEXT("p.p.s. in the dark it cant see you. not at all. it only sees the light.\n\nso in the dark it listens. hold your breath (F) and creep and it goes right past you. not for long though. your heart gets loud and then you have to breathe.\n\nwhen things in the house move on their own, it is coming.\n\nthe house plays tricks too. if the flame doesnt shake, it isnt close.\n\nthe flame knows the way. a slow lean, follow it. a hard cold bend is IT."))};
if(Key==TEXT("register")) return {FText::FromString(TEXT("PRYCE. PRIVATE REGISTER.\n\nThomas Pryce. Aged seven.\nDIED IN THE FLUE, 1866.\n\nThe first entry is crossed out.\n\nR.P. — UNSUITABLE.")),FText::FromString(TEXT("NAMES. DATES. TREATMENTS.\n\n1894. Alfred Morgan. FINISHED.\n1894. Elsie Jenkins. RETURNED.\n1932. A new page. A new child.\n1958. The same careful hand.\n1973. Still the same hand.\n\nJosiah could not have written them all.")),FText::FromString(TEXT("Winter, 1974.\n\nMAISIE EVANS.\nNOT FINISHED. GONE.\n\nUnderneath, written more slowly:\n\nMAISIE EVANS. FINISHED.\n\nELEANOR PRICE. PLACE PREPARED."))};
if(Key==TEXT("chart")) return {FText::FromString(TEXT("TOM 4. TOM 5. TOM 6. TOM 7.\n\nHis marks stop at seven.\n\nOther names begin below them. Dates from different decades. Small soot-black handprints.\n\nThe higher marks are not a child growing. They are different children standing in the same place."))};
if(Key==TEXT("bill")) return {FText::FromString(TEXT("MISSING.\nWILLIAM REES. AGED SEVEN.\n\nH Division, Bethnal Green. 1894.\n\nSomeone kept this notice for eighty years.\n\nUnderneath, in pencil:\nfound him. he's good now."))};
if(Key==TEXT("newspaper")) return {FText::FromString(TEXT("EAST LONDON. WINTER 1974.\n\nANOTHER MISSING CHILD RETURNS.\n\nThe latest disappearance follows incidents recorded in 1894, 1932, 1958 and last winter. Children return clean, fed, and unable to account for their absence. Parents describe remarkable improvements in behaviour.")),FText::FromString(TEXT("In the margin:\n\nImprovement is not the same as recovery.\n\nShe used to laugh without being asked."))};
if(Key==TEXT("notebook")) return {FText::FromString(TEXT("H DIVISION. BETHNAL GREEN.\n1894.\n\nA child described a chair, a pale face and something soft moving above her.\n\nThe constable wrote:\nChild's fancy. No further action.")),FText::FromString(TEXT("Many years later, on the next page:\n\nI was there. And I did nothing.\n\nHe had been warned long before Nell was born."))};
if(Key==TEXT("nan")) return {FText::FromString(TEXT("Nell love,\n\nA little pale light means a child's funeral. A corpse-candle.\n\nNever follow it.\n\nAnd don't let anyone tell you there is something wrong with being frightened.\n\nCome home.\nNan x"))};
if(Key==TEXT("drawings")) return {FText::FromString(TEXT("A huge bed. A door stretched taller than the wall. A corridor with no end.\n\nThe drawings are from different decades. The rooms around you look like them.\n\nMim understands a drawing better than it understands a house.")),FText::FromString(TEXT("Ten arms. Two great eyes. An impossible smile.\n\nSome children draw Mim holding their hands.\n\nOthers draw themselves inside its body.\n\nThey say they do not remember it."))};
if(Key==TEXT("labels")) return {FText::FromString(TEXT("Names. Dates. Ages.\n\nNot memorials. A record of what was taken.\n\nFEAR. HOMESICKNESS. ANGER. REFUSAL.\n\nThe feelings were preserved in the tallow. When it burns, something of the child escapes.")),FText::FromString(TEXT("MAISIE EVANS. FINISHED.\n\nBeside her place, an empty mould:\n\nELEANOR PRICE.\n\nThe light that protected you was made from something stolen."))};
if(Key==TEXT("ruth")) return {FText::FromString(TEXT("For our Thomas.\n\nYou needn't be brave all the time.\n\nLove, Mother.\n\nBlack fabric. Unfinished animals. A needle still in a sleeve.\n\nRuth's name vanishes from the records. The register says only R.P. — UNSUITABLE."))};
if(Key==TEXT("portrait")) return {FText::FromString(TEXT("JOSIAH, RUTH AND THOMAS PRYCE.\nSPITALFIELDS, 1866.\n\nOur Thomas. Such a brave boy.\n\nA later note:\nAged seven. Trapped in a chimney for two days. His candle went out.\n\nHe did not survive.")),FText::FromString(TEXT("Josiah blamed the fear.\n\nNot the men who sent a child into a chimney.\n\nThe face of Ruth's unfinished toy was taken from a cast associated with Thomas.\n\nMim is not Josiah.\nJosiah died. Mim kept working."))};
if(Key==TEXT("letter")) return {FText::FromString(TEXT("Dear Elsie,\n\nI thought I was keeping you safe.\n\nI took your hand and walked you to the house myself.\n\nYou asked if I was coming inside. I told you to be good.\n\nI was there. And I did nothing.\n\nYour father."))};
if(Key==TEXT("maisie")) return {FText::FromString(TEXT("The first time I ran because I was frightened.\n\nThe second time I knew what was there.\n\nI should have run again.\n\nBut I didn't want to anymore.")),FText::FromString(TEXT("nell\n\nwhen you arent afraid of something bad you dont run.\n\nwhen you arent angry you dont fight.\n\nbeing quiet isnt being happy.\n\ndont let it finish you.\n\nmaisie"))};
return {}; }
}
