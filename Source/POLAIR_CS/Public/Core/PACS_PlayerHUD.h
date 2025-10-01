#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PACS_PlayerHUD.generated.h"

/**
 * HUD class for POLAIR_CS that handles marquee selection visualization
 * Client-side only rendering, VR-aware (auto-disabled on HMD clients)
 */
UCLASS()
class POLAIR_CS_API APACS_PlayerHUD : public AHUD
{
	GENERATED_BODY()

protected:
	// Override HUD drawing
	virtual void DrawHUD() override;

private:
	// Helper to draw the marquee rectangle with fill and border
	void DrawMarqueeRectangle(const FVector2D& Start, const FVector2D& End);

	// Marquee visual settings
	UPROPERTY(EditDefaultsOnly, Category = "Marquee")
	FLinearColor MarqueeFillColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.15f);

	UPROPERTY(EditDefaultsOnly, Category = "Marquee")
	FLinearColor MarqueeBorderColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Marquee")
	float MarqueeBorderThickness = 2.0f;
};