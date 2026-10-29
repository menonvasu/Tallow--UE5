#pragma once
#include "CoreMinimal.h"
namespace TallowNarrative { TALLOW_API TArray<FName> Keys(); TALLOW_API FText Title(FName Key); TALLOW_API bool Printed(FName Key); TALLOW_API TArray<FText> Pages(FName Key); }
