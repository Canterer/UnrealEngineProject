// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpModule.h"
#include "IPlatformFilePak.h"
#include "AsyncTaskDownloadAsset.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDownloadPakDelegate);
UCLASS()
class WEWORLD_API AAsyncTaskDownloadAsset : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAsyncTaskDownloadAsset();

	FDownloadPakDelegate OnPakDownloadOk;


	FString PakURL;//Resource package network path;
	FString PakStoragePath; //Resource package local storage path;

	FString PakName;//The package name of the resource;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	TSharedPtr<FPakPlatformFile> PakPlatformFile;
	class IPlatformFile* OldPlatformFile;
public:
	void AppendBytesToFile(const FString& FilePath, const TArray<uint8>& Bytes);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void StartTask();

	void LoadPak();

	void ManifestDownloadOkHandle(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	void ManifestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived);

	void PatchDownloadOkHandle(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	void PatchDownloadProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived);

	void HandleRequest(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	void RequestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived);

	UFUNCTION(BlueprintImplementableEvent,Category="HotPatch")
	void OnVersionSupport(bool bVersionSupport);

	UFUNCTION(BlueprintImplementableEvent,Category="HotPatch")
	void GetManifestDownloadStat(int32 BytesSent, int32 BytesReceived);

	UFUNCTION(BlueprintImplementableEvent,Category="HotPatch")
	void GetPatchDownloadStat(int32 BytesSent, int32 BytesReceived);
	
	UFUNCTION(BlueprintCallable,Category="HotPatch")
	void DoPatchDownload();

	UFUNCTION(BlueprintImplementableEvent,Category="HotPatch")
	void OnPatchDownloadComplete();

	UFUNCTION(BlueprintImplementableEvent, Category = "HotPatch")
	void OnNetBreak();

	UFUNCTION(BlueprintCallable,Category="HotPatch")
	void CheckGameVersion();
	
	UFUNCTION(BlueprintCallable,Category="HotPatch")
    void UnitTest();

	UFUNCTION(BlueprintCallable, Category = "HotPatch")
	bool IsNetworkAvailable();

	void DownloadFile(const FString& URL, const FString& FileSavePath,int32 FileTotalSize,int32 ChunkSize);

	UFUNCTION(CallInEditor, Category = "HotPatch")
	void BeginUnitTest();

	UFUNCTION(CallInEditor, Category = "HotPatch")
	void EndUnitTest();

	void CleanupPatchCache();
	void DeleteDirectoryRecursively(const FString& InDirectory);

	TSharedPtr<IHttpRequest> MyHttpRequest;

	TArray<FString> PatchUrlList;
	TArray<FString> NeedDownloadFileNames;
	TArray<FString> BasePakNameList;
	FString CurrentPatchName;
	FString PlatformBasePakName;

	FString PatchDir;
	
	TArray<FString> Urls;
	bool bNeedDownloadBasePak=false;
	uint32 BasePakDownloadProgress=0;
	uint32 PatchPakDownloadProgress=0;

	FString OfficialVersion;

	bool bNetConnect=false;
	FTimerHandle NetTimerHandle;

	//continue Download
	bool bSingleFileDownloadOk = false;
	bool bCanChunkDownload = false;
	bool bCancelDownload=false;
	TMap<FString, int64> FileSizeMap;
	FString ResumeUrl;
	float NetCheckTime = 0;
	bool bCheckNetConnectWait = false;

	
	

	UPROPERTY(BlueprintReadOnly,Category="HotPatch")
	int32 CurrentDownloadSize=0;

	int32 FileStepDownloadSize = 0;

	UPROPERTY(BlueprintReadOnly,Category="HotPatch")
	int32 TotalSize=0;

	UPROPERTY(BlueprintReadOnly,Category="HotPatch")
	bool bBeginDownloadPatch=false;
	
	UPROPERTY(BlueprintReadWrite,Category="HotPatch")
	FString BaseUrlDir;

	UPROPERTY(BlueprintReadWrite,Category="HotPatch")
	FString  PlatformUrlDir;
	
	UPROPERTY(BlueprintReadWrite,Category="HotPatch")
	bool bBeginNextURLRequest = false;

	UFUNCTION(BlueprintCallable,Category="HotPatch")
	void RecursionDownloadUrls();

	UFUNCTION(BlueprintCallable, Category = "HotPatch")
	void ResumeDownload();

	void CheckInternetConnection();

	void NetCheckHandle(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
};
