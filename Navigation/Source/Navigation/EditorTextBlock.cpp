// Fill out your copyright notice in the Description page of Project Settings.


#include "EditorTextBlock.h"

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

void UEditorTextBlock::SynchronizeProperties()
{
	SynchronizeProperties_BP();
	Super::SynchronizeProperties();
}