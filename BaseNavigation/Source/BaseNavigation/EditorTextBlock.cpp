// Fill out your copyright notice in the Description page of Project Settings.

#include "EditorTextBlock.h"

void UEditorTextBlock::SynchronizeProperties()
{
	SynchronizeProperties_BP();
	Super::SynchronizeProperties();
}