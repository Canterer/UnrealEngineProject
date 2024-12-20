// Fill out your copyright notice in the Description page of Project Settings.


#include "ScrollCanvasPanel.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"

TSharedRef<SWidget> UScrollCanvasPanel::RebuildWidget()
{
    Super::RebuildWidget();
    MyCanvas->SetOnMouseButtonDown(BIND_UOBJECT_DELEGATE(FPointerEventHandler, OnMouseButtonDown));
	MyCanvas->SetOnMouseButtonUp(BIND_UOBJECT_DELEGATE(FPointerEventHandler, OnMouseButtonUp));
	MyCanvas->SetOnMouseMove(BIND_UOBJECT_DELEGATE(FPointerEventHandler, OnMouseMove));
	//MyCanvas->SetOnMouseEnter(BIND_UOBJECT_DELEGATE(FNoReplyPointerEventHandler, OnMouseEnter));
	MyCanvas->SetOnMouseLeave(BIND_UOBJECT_DELEGATE(FSimpleNoReplyPointerEventHandler, OnMouseLeave));
    return MyCanvas.ToSharedRef();
}

FReply UScrollCanvasPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    /*
    if (bClickFuncTag)
    {
        UE_LOG(LogTemp, Log, TEXT("OnMouseButtonDown bClickFuncTag true"));
        return FReply::Unhandled();
    }
    int pointIndex = MouseEvent.GetPointerIndex();
    FVector2D pos = MouseEvent.GetScreenSpacePosition();
    FVector2D local = MyGeometry.LocalToAbsolute(FVector2D(0, 0));
    const float scale = UWidgetLayoutLibrary::GetViewportScale(this);
    //UE_LOG(LogTemp, Log, TEXT("OnMouseButtonDown FVector2D: %f %f local: %f %f scale: %f"), pos.X, pos.Y, local.X, local.Y, scale);
    pointerDownPos = (pos - local) / scale;
    //UE_LOG(LogTemp, Log, TEXT("OnMouseButtonDown pointIndex:%d FVector2D: %f %f"), pointIndex, pos.X, pos.Y);
    bNeedMouseMove = true;
    pointerPosMap.Add(pointIndex, pos);
    if (pointerPosMap.Num() >= 2)
    {//缩放为触屏双指事件 pointIndex为0 1
        scaleVecLength = FVector2D::Distance(pointerPosMap[0], pointerPosMap[1]);
        scaleCenter = (pointerPosMap[0] + pointerPosMap[1])/scale/2;
        //UE_LOG(LogTemp, Log, TEXT("scaleCenter FVector2D: %f %f FVector2D: %f %f"), scaleCenter.X, scaleCenter.Y, pointerPosMap[0].X, pointerPosMap[0].Y, pointerPosMap[1].X, pointerPosMap[1].Y);
        onScaleEvent.Broadcast(scaleCenter, 1, true);
    }

    return FReply::Handled();
    */
    if (bClickFuncTag)
    {
        UE_LOG(LogTemp, Log, TEXT("OnMouseButtonDown bClickFuncTag true"));
        return FReply::Handled();
    }
    int pointIndex = MouseEvent.GetPointerIndex();
    FVector2D pos = MouseEvent.GetScreenSpacePosition();
    pointerDownPos = pos;
    LastMouseEvent = MouseEvent;
    TouchTime = UGameplayStatics::GetTimeSeconds(GetWorld());
    UE_LOG(LogTemp, Log, TEXT("OnMouseButtonDown pointIndex:%d FVector2D: %f %f"), pointIndex, pos.X, pos.Y);
    bNeedMouseMove = true;
    if (pointerPosMap.Num() >= 2)
    {
        //缩放为触屏双指事件 pointerIndex 0 1
        TArray<int> keyArray;
        pointerPosMap.GenerateKeyArray(keyArray);
        keyArray.Sort([](const int& a, const int& b) { return a < b; });
        int firstIndex = keyArray[0];
        int secondIndex = keyArray[1];

        scaleVecLength = FVector2D::Distance(pointerPosMap[firstIndex], pointerPosMap[secondIndex]);
        scaleCenter = (pointerPosMap[firstIndex] + pointerPosMap[secondIndex]) / 2;
        onScaleEvent.Broadcast(scaleCenter, 1, true);
    }
    return FReply::Handled().CaptureMouse(this->TakeWidget());
    
}
FReply UScrollCanvasPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    /*
    int pointIndex = MouseEvent.GetPointerIndex();
    FVector2D pos = MouseEvent.GetScreenSpacePosition();
    FVector2D local = MyGeometry.LocalToAbsolute(FVector2D(0, 0));
    const float scale = UWidgetLayoutLibrary::GetViewportScale(this);
    UE_LOG(LogTemp, Log, TEXT("OnMouseButtonUp FVector2D: %f %f local: %f %f scale: %f"), pos.X, pos.Y, local.X, local.Y, scale);
    pos = (pos - local) / scale;
    bNeedMouseMove = false;
    pointerPosMap.Remove(pointIndex);
    UE_LOG(LogTemp, Log, TEXT("OnMouseButtonUp pointIndex:%d FVector2D: %f %f"), pointIndex, pos.X, pos.Y);
    if (FVector2D::Distance(pos, pointerDownPos) < 20)
    {
        onClickEvent.Broadcast(pos);
    }else
        UE_LOG(LogTemp, Log, TEXT("OnMouseButtonUp pointIndex:%d distance:%f FVector2D: %f %f FVector2D: %f %f"), pointIndex, FVector2D::Distance(pos, pointerDownPos), pointerDownPos.X, pointerDownPos.Y, pos.X, pos.Y);

    onMoveUpEvent.Broadcast(bFakeEvent);
    if(bFakeEvent)
        bFakeEvent = false;
    return FReply::Handled();
    */
    int pointIndex = MouseEvent.GetPointerIndex();
    FVector2D pos = MouseEvent.GetScreenSpacePosition();
    bNeedMouseMove = false;
    bButtonPressMouseMove = false;
    pointerPosMap.Remove(pointIndex);
    LastMouseUpEvent = MouseEvent;
    UE_LOG(LogTemp, Log, TEXT("OnMouseButtonUp pointerIndex:%d FVector2D: %f %f"), pointIndex, pos.X, pos.Y);
    if (bClickFuncTag)
    {
        bClickFuncTag = false;
        UE_LOG(LogTemp, Log, TEXT("OnMouseButtonUp bClickFuncTag false"));
        return FReply::Handled();
    }
    float CurrentTime = UGameplayStatics::GetTimeSeconds(GetWorld());
    float OffsetTime = CurrentTime - TouchTime;
    if (OffsetTime > 0 && OffsetTime < 0.2)
    {
        onClickEvent.Broadcast(pointerDownPos);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("OnMouseButtonUp pointIndex:%d distance:%f FVector2D: %f %f FVector2D: %f %f"), pointIndex, FVector2D::Distance(pos, pointerDownPos), pointerDownPos.X, pointerDownPos.Y, pos.X, pos.Y);
        onMoveUpEvent.Broadcast(bFakeEvent);
        if (bFakeEvent)
            bFakeEvent = false;
    }
    return FReply::Handled().ReleaseMouseCapture();
}

void UScrollCanvasPanel::OnSeatButtonClick()
{
    bNeedMouseMove = false;
    bButtonPressMouseMove = false;
    pointerPosMap.Empty();
}

void UScrollCanvasPanel::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    int pointIndex = MouseEvent.GetPointerIndex();
    FVector2D pos = MouseEvent.GetScreenSpacePosition();
    pointerPosMap.Remove(pointIndex);
    //UE_LOG(LogTemp, Log, TEXT("OnMouseEnter pointIndex:%d FVector2D: %f %f"), pointIndex, pos.X, pos.Y);
}
FReply UScrollCanvasPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    /*
    if(!bNeedMouseMove)
        return FReply::Unhandled();
    int pointIndex = MouseEvent.GetPointerIndex();
    FVector2D pos = MouseEvent.GetScreenSpacePosition();
    FVector2D tempLastPos = pointerPosMap[pointIndex];
    pointerPosMap[pointIndex] = pos;
    if( pointerPosMap.Num() >= 2)
    {//缩放为触屏双指事件 pointIndex为0 1
        float vecLength = FVector2D::Distance(pointerPosMap[0], pointerPosMap[1]);
        //UE_LOG(LogTemp, Log, TEXT("OnMouseMove scale: %f %f %f"), vecLength/scaleVecLength, scaleVecLength, vecLength);
        onScaleEvent.Broadcast(scaleCenter, vecLength/scaleVecLength, false);
    }else if(bNeedMouseMove)
    {
        //UE_LOG(LogTemp, Log, TEXT("OnMouseMove"));
        onMoveEvent.Broadcast(pos - tempLastPos);
    }
    return FReply::Handled();
    */
    if (!bNeedMouseMove && !bButtonPressMouseMove)
    {
        return FReply::Handled();
    }

    int pointIndex = MouseEvent.GetPointerIndex();
    FVector2D pos = MouseEvent.GetScreenSpacePosition();
    if (!pointerPosMap.Contains(pointIndex))
    {
        if (bButtonPressMouseMove)
            pointerPosMap.Add(pointIndex, pos);
        return FReply::Handled();
    }

    FVector2D tempLastPos = pointerPosMap[pointIndex];
    pointerPosMap[pointIndex] = pos;
    if (pointerPosMap.Num() >= 2)
    {
        //缩放为触屏双指事件 pointIndex 0 1
        TArray<int> keyArray;
        pointerPosMap.GenerateKeyArray(keyArray);
        keyArray.Sort([](const int& a, const int& b) {return a < b; });
        int firstIndex = keyArray[0];
        int secondIndex = keyArray[1];
        float vecLength = FVector2D::Distance(pointerPosMap[firstIndex], pointerPosMap[secondIndex]);
        onScaleEvent.Broadcast(scaleCenter, vecLength / scaleVecLength, false);
    }
    else if (bNeedMouseMove || bButtonPressMouseMove)
    {
        onMoveEvent.Broadcast(pos - tempLastPos);
    }
    return FReply::Handled();
}
void UScrollCanvasPanel::OnMouseLeave(const FPointerEvent& MouseEvent)
{
    int pointIndex = MouseEvent.GetPointerIndex();
    FVector2D pos = MouseEvent.GetScreenSpacePosition();
    //UE_LOG(LogTemp, Log, TEXT("OnMouseLeave pointIndex:%d FVector2D: %f %f"), pointIndex, pos.X, pos.Y);
    bNeedMouseMove = false;
    bButtonPressMouseMove = false;
}

void UScrollCanvasPanel::OnClickFunc()
{
    /*
    UE_LOG(LogTemp, Log, TEXT("OnClickFunc begin"));
    //UE_LOG(LogTemp, Log, TEXT("OnClickFunc bClickFuncFlag %d selfVisibility %d ChildVisibility %d ESlateVisibility::Visible %d ESlateVisibility::SelfHitTestInvisible %d"), bClickFuncFlag, this->GetVisibility(), clickChild->GetVisibility(), ESlateVisibility::Visible, ESlateVisibility::SelfHitTestInvisible);
    bClickFuncTag = true;
    OnClickDown();
    OnClickUp();
    bClickFuncTag = false;
    OnClickUp();
    UE_LOG(LogTemp, Log, TEXT("OnClickFunc end"));
    */

    bClickFuncTag = true;
    OnClickDown(LastMouseEvent);
}

void UScrollCanvasPanel::OnClickUpFunc()
{
    bClickFuncTag = true;
    OnClickUp(LastMouseUpEvent);
    bClickFuncTag = false;
}

void UScrollCanvasPanel::OnClickDown(const FPointerEvent& MouseEvent)
{
    /*
    UE_LOG(LogTemp, Log, TEXT("OnClickDown begine"));
    FSlateApplication& SlateApp = FSlateApplication::Get();
    const TSharedPtr< FGenericWindow > GenWindow = GEngine->GameViewport->GetWindow()->GetNativeWindow();
    //FPointerEvent MouseDownEvent(0, SlateApp.CursorPointerIndex, SlateApp.GetCursorPos(), SlateApp.GetLastCursorPos(),
    //    SlateApp.GetPressedMouseButtons(), EKeys::LeftMouseButton, 0, SlateApp.GetPlatformApplication()->GetModifierKeys());
    //SlateApp.ProcessMouseButtonDownEvent(GenWindow, MouseDownEvent);
    SlateApp.OnMouseDown(GenWindow, EMouseButtons::Left);
    UE_LOG(LogTemp, Log, TEXT("OnClickDown end"));
    */

    FSlateApplication& SlateApp = FSlateApplication::Get();
    const TSharedPtr<FGenericWindow> GenWindow = GEngine->GameViewport->GetWindow()->GetNativeWindow();
    if (SlateApp.IsFakingTouchEvents())
    {
        UE_LOG(LogTemp, Log, TEXT("OnTouchStarted"));
        SlateApp.OnTouchStarted(GenWindow, MouseEvent.GetScreenSpacePosition(), 1.0f, 0, FSlateApplicationBase::SlateAppPrimaryPlatformUser, IPlatformInputDeviceMapper::Get().GetDefaultInputDevice());
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("OnClickDown"));
        SlateApp.ProcessMouseButtonDownEvent(GenWindow, MouseEvent);
    }
}

void UScrollCanvasPanel::OnClickUp(const FPointerEvent& MouseEvent)
{
    /*
    UE_LOG(LogTemp, Log, TEXT("OnClickUp begine"));
    bFakeEvent = true;
    FSlateApplication& SlateApp = FSlateApplication::Get();
    SlateApp.OnMouseUp(EMouseButtons::Left);
    UE_LOG(LogTemp, Log, TEXT("OnClickUp end"));
    */
    bFakeEvent = true;
    FSlateApplication& SlateApp = FSlateApplication::Get();
    if (SlateApp.IsFakingTouchEvents())
    {
        UE_LOG(LogTemp, Log, TEXT("OnTouchEnd"));
        SlateApp.OnTouchEnded(MouseEvent.GetScreenSpacePosition(), 0, FSlateApplicationBase::SlateAppPrimaryPlatformUser, IPlatformInputDeviceMapper::Get().GetDefaultInputDevice());
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("OnClickUp"));
        SlateApp.ProcessMouseButtonUpEvent(MouseEvent);
    }
}