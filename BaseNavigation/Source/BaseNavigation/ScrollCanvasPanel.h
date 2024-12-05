// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/CanvasPanel.h"
#include "ScrollCanvasPanel.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoveUpEvent, bool, bFakeEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClickEvent, FVector2D, pos);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoveEvent, FVector2D, moveVec);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnScaleEvent, FVector2D, scaleCenter, float, scaleRate, bool, bStart);
UCLASS()
class BASENAVIGATION_API UScrollCanvasPanel : public UCanvasPanel
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintAssignable)
		FOnMoveUpEvent onMoveUpEvent;
	UPROPERTY(BlueprintAssignable)
		FOnClickEvent onClickEvent;
	UPROPERTY(BlueprintAssignable)
		FOnMoveEvent onMoveEvent;
	UPROPERTY(BlueprintAssignable)
		FOnScaleEvent onScaleEvent;


	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent);

	UFUNCTION(BlueprintCallable)
		void OnClickDown(const FPointerEvent& MouseEvent);
	UFUNCTION(BlueprintCallable)
		void OnClickUp(const FPointerEvent& MouseEvent);
	UFUNCTION(BlueprintCallable)
		void OnClickFunc();
	UFUNCTION(BlueprintCallable)
		void OnClickUpFunc();

	UFUNCTION(BlueprintCallable)
		void OnSeatButtonClick();//座位按钮点击需要清理
	bool bClickFuncTag = false;
	UPROPERTY(BlueprintReadWrite)
		bool bButtonPressMouseMove = false;
protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	FVector2D scaleCenter;
	float scaleVecLength;
	FVector2D pointerDownPos;
	TMap<int, FVector2D> pointerPosMap;
	bool bNeedMouseMove = false;
	bool bFakeEvent = false;

	UPROPERTY(BlueprintReadWrite)
		FPointerEvent LastMouseEvent;
	UPROPERTY(BlueprintReadWrite)
		FPointerEvent LastMouseUpEvent;
	float TouchTime;
};
