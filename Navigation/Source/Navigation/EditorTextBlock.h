// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "EditorTextBlock.generated.h"

/**
 * 
 */
UCLASS()
class NAVIGATION_API UEditorTextBlock : public UTextBlock
{
	GENERATED_BODY()
public:
	virtual void SynchronizeProperties();

	UFUNCTION(BlueprintImplementableEvent)
		void SynchronizeProperties_BP();
};

