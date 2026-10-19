#if WITH_EDITOR
#include "UI/Presentation/TallowMenuWidget.h"
#include "UI/Presentation/TallowLessonWidget.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/BufferArchive.h"
#include "Slate/WidgetRenderer.h"
#include "UnrealClient.h"

// Render the actual native widgets for layout review. Does not create or alter a game world.
namespace TallowPresentationPreview
{
	TWeakObjectPtr<UTallowMenuWidget> ViewportMenu;

	void ShowViewportMenu(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld()
			|| FPackageName::GetLongPackagePath(World->GetOutermost()->GetName()) != TEXT("/Game/Presentation/Cinematics/Maps"))
		{
			UE_LOG(LogTemp, Warning, TEXT("MenuFixture requires PIE in a presentation cinematic map."));
			return;
		}
		if (ViewportMenu.IsValid()) ViewportMenu->RemoveFromParent();
		ViewportMenu.Reset();
		if (Args.Num() && Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase)) return;
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC || !PC->IsLocalController()) return;
		UTallowMenuWidget* Menu = CreateWidget<UTallowMenuWidget>(PC, UTallowMenuWidget::StaticClass());
		if (!Menu) return;
		Menu->ShowMenu(Args.Num() && Args[0].Equals(TEXT("outro"), ESearchCase::IgnoreCase)
			? ETallowMenuMode::Outro : ETallowMenuMode::Main);
		Menu->AddToViewport(20);
		ViewportMenu = Menu;
		UE_LOG(LogTemp, Display, TEXT("MenuFixture ready with the production menu widget."));
	}
	FAutoConsoleCommandWithWorldAndArgs ViewportCommand(TEXT("Tallow.Presentation.MenuFixture"),
		TEXT("Editor cinematic review maps only: show main/outro menu, or off."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ShowViewportMenu));
	FAutoConsoleCommandWithWorldAndArgs CaptureCommand(TEXT("Tallow.Presentation.CaptureViewport"),
		TEXT("Editor presentation maps only: capture the actual viewport and UI to a Saved/Presentation file."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || !World->IsGameWorld() || Args.Num() != 1
				|| !World->GetOutermost()->GetName().StartsWith(TEXT("/Game/Presentation/"))) return;
			const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Presentation/"));
			const FString Filename = FPaths::ConvertRelativePathToFull(Args[0]);
			if (!FPaths::IsUnderDirectory(Filename, Directory)) return;
			FScreenshotRequest::RequestScreenshot(Filename, true, false, false, FIntRect(), true);
		}));

	bool Save(UUserWidget* Widget, const FString& Name, const FVector2D& Size)
	{
		FWidgetRenderer Renderer(false, true);
		UTextureRenderTarget2D* Target = Renderer.DrawWidget(Widget->TakeWidget(), Size);
		if (!Target) return false;
		FBufferArchive Bytes;
		if (!FImageUtils::ExportRenderTarget2DAsPNG(Target, Bytes)) return false;
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Presentation/Preview");
		IFileManager::Get().MakeDirectory(*Directory, true);
		return FFileHelper::SaveArrayToFile(Bytes, *(Directory / (Name + TEXT(".png"))));
	}

	void Export()
	{
		if (!FApp::CanEverRender())
		{
			UE_LOG(LogTemp, Error, TEXT("TALLOW UI preview needs a rendering RHI; omit -nullrhi."));
			return;
		}
		UTallowMenuWidget* Menu = NewObject<UTallowMenuWidget>();
		Menu->AddToRoot();
		Menu->Initialize();
		bool bOK = Save(Menu, TEXT("menu-main-720"), FVector2D(1280, 720));
		bOK &= Save(Menu, TEXT("menu-main-360"), FVector2D(640, 360));
		Menu->ActivateCommand(ETallowMenuCommand::Controls);
		bOK &= Save(Menu, TEXT("menu-controls-720"), FVector2D(1280, 720));
		Menu->ShowMenu(ETallowMenuMode::Pause);
		Menu->ActivateCommand(ETallowMenuCommand::Restart);
		bOK &= Save(Menu, TEXT("menu-confirm-720"), FVector2D(1280, 720));
		Menu->ShowMenu(ETallowMenuMode::Outro);
		bOK &= Save(Menu, TEXT("menu-outro-720"), FVector2D(1280, 720));
		Menu->RemoveFromRoot();
		UTallowLessonWidget* Lesson = NewObject<UTallowLessonWidget>();
		Lesson->AddToRoot();
		Lesson->Initialize();
		Lesson->ShowLesson(ETallowLesson::Cup);
		bOK &= Save(Lesson, TEXT("lesson-cup-720"), FVector2D(1280, 720));
		Lesson->RemoveFromRoot();
		UE_LOG(LogTemp, Display, TEXT("TALLOW UI preview export: %s"), bOK ? TEXT("PASS") : TEXT("FAIL"));
	}
	FAutoConsoleCommand ExportCommand(TEXT("Tallow.Presentation.ExportUI"),
		TEXT("Export native presentation widgets to Saved/Presentation/Preview (editor only)."),
		FConsoleCommandDelegate::CreateStatic(&Export));
}
#endif
