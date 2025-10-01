#include "Core/PACS_PlayerHUD.h"
#include "Core/PACS_PlayerController.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/Canvas.h"

void APACS_PlayerHUD::DrawHUD()
{
	Super::DrawHUD();

	// Skip marquee drawing on VR/HMD clients
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		return;
	}

	// Get owning player controller and check if marquee is active
	APACS_PlayerController* PC = Cast<APACS_PlayerController>(GetOwningPlayerController());
	if (!PC || !PC->IsMarqueeActive())
	{
		return;
	}

	// Draw the marquee rectangle
	DrawMarqueeRectangle(PC->GetMarqueeStartPos(), PC->GetMarqueeCurrentPos());
}

void APACS_PlayerHUD::DrawMarqueeRectangle(const FVector2D& Start, const FVector2D& End)
{
	if (!Canvas)
	{
		return;
	}

	// Calculate rectangle bounds ensuring positive dimensions
	const float X = FMath::Min(Start.X, End.X);
	const float Y = FMath::Min(Start.Y, End.Y);
	const float Width = FMath::Abs(End.X - Start.X);
	const float Height = FMath::Abs(End.Y - Start.Y);

	// Skip drawing if rectangle is too small (avoid single-pixel artifacts)
	if (Width < 2.0f || Height < 2.0f)
	{
		return;
	}

	// Draw semi-transparent fill
	DrawRect(MarqueeFillColor, X, Y, Width, Height);

	// Draw solid border lines
	// Top edge
	DrawLine(Start.X, Start.Y, End.X, Start.Y, MarqueeBorderColor, MarqueeBorderThickness);
	// Right edge
	DrawLine(End.X, Start.Y, End.X, End.Y, MarqueeBorderColor, MarqueeBorderThickness);
	// Bottom edge
	DrawLine(End.X, End.Y, Start.X, End.Y, MarqueeBorderColor, MarqueeBorderThickness);
	// Left edge
	DrawLine(Start.X, End.Y, Start.X, Start.Y, MarqueeBorderColor, MarqueeBorderThickness);
}