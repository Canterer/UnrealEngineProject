// Fill out your copyright notice in the Description page of Project Settings.


#include "AsyncTaskDownloadAsset.h"
#include "WeWorldBlueprintFunctionLibrary.h"
#include "Engine/StreamableManager.h"
#include "Kismet/KismetStringLibrary.h"
#include "IPlatformFilePak.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Engine/StaticMeshActor.h"
#include "HttpModule.h"
#include "Async/Async.h"
#include "DSP/ModulationMatrix.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Sets default values
AAsyncTaskDownloadAsset::AAsyncTaskDownloadAsset()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	/*这部分放蓝图实现*/
	// 返回值: Windows, Android,HTML5,IOS
	// FString PlatformName=FPlatformProperties::IniPlatformName();
	// BaseUrlDir=TEXT("https://da-1258470876.cos.ap-guangzhou.myqcloud.com/ww_resource/");
	// PlatformUrlDir = BaseUrlDir+PlatformName+TEXT("/");

	//BaseUrlDir = TEXT("https://weworld-1257223132.cos.ap-guangzhou.myqcloud.com/prd/PatchPaks/PatchPak/");
	//BaseUrlDir = TEXT("http://43.136.71.116/NewPak/");
	FString ProjectName = FPaths::ProjectDir();
	ProjectName.RemoveAt(ProjectName.Len() - 1);
	int32 index;
	ProjectName.FindLastChar('/', index);
	ProjectName = ProjectName.Right(ProjectName.Len() - index - 1);

	PatchDir=FPaths::ProjectDir()+TEXT("Patchs/");
	if (!FPaths::DirectoryExists(PatchDir))
	{
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*PatchDir);
	}

#if PLATFORM_ANDROID
	//BasePakNameList.Add(TEXT("global.ucas"));
	//BasePakNameList.Add(TEXT("global.utoc"));
	BasePakNameList.Add(ProjectName + TEXT("-Android_ETC2.pak"));
	//BasePakNameList.Add(ProjectName+TEXT("-Android_ETC2.ucas"));
	//BasePakNameList.Add(ProjectName+TEXT("-Android_ETC2.utoc"));

	PlatformBasePakName = TEXT("BasePakSize_Android");
#endif

#if PLATFORM_WINDOWS
	//BasePakNameList.Add(TEXT("global.ucas"));
	//BasePakNameList.Add(TEXT("global.utoc"));
	BasePakNameList.Add(ProjectName + TEXT("-Windows.pak"));
	//BasePakNameList.Add(ProjectName+TEXT("-Windows.ucas"));
	//BasePakNameList.Add(ProjectName+TEXT("-Windows.utoc"));

	PlatformBasePakName = TEXT("BasePakSize_Windows");
#endif

#if PLATFORM_IOS
	//BasePakNameList.Add(TEXT("global.ucas"));
	//BasePakNameList.Add(TEXT("global.utoc"));
	BasePakNameList.Add(ProjectName + TEXT("-ios.pak"));
	//BasePakNameList.Add(ProjectName+TEXT("-ios.ucas"));
	//BasePakNameList.Add(ProjectName+TEXT("-ios.utoc"));

	PlatformBasePakName = TEXT("BasePakSize_IOS");
#endif

#if PLATFORM_MAC
	//BasePakNameList.Add(TEXT("global.ucas"));
	//BasePakNameList.Add(TEXT("global.utoc"));
	BasePakNameList.Add(ProjectName + TEXT("-Mac.pak"));
	//BasePakNameList.Add(ProjectName+TEXT("-Mac.ucas"));
	//BasePakNameList.Add(ProjectName+TEXT("-Mac.utoc"));

	PlatformBasePakName = TEXT("BasePakSize_Mac");
#endif
}

// Called when the game starts or when spawned
void AAsyncTaskDownloadAsset::BeginPlay()
{
	Super::BeginPlay();

	CleanupPatchCache();
	//**this Timer crash in Meta50**//
	// 
	//GetWorld()->GetTimerManager().SetTimer(NetTimerHandle, this, &AAsyncTaskDownloadAsset::CheckInternetConnection,3.0f, true);
	//
	//**this Timer crash in Meta50**//

	bBeginNextURLRequest = false;

	//FString TestUrl = TEXT("http://43.136.71.116/NewPak/TestDownloads.rar");
	////ChunkSize=1MB
	//DownloadFile(TestUrl, TEXT("D:/Test/111.rar"), 207132580, 1048576);

	if (FCoreDelegates::OnMountAllPakFiles.IsBound())
	{
		TArray<FString> PakFolders;
		//PakFolders.Add(FPaths::ProjectSavedDir() + TEXT("Paks/"));
		PakFolders.Add(PatchDir);
		FCoreDelegates::OnMountAllPakFiles.Execute(PakFolders);
	}
}

void AAsyncTaskDownloadAsset::AppendBytesToFile(const FString& FilePath, const TArray<uint8>& Bytes)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle* FileHandle = PlatformFile.OpenWrite(*FilePath, true, false);
	if (FileHandle)
	{
		FileHandle->SeekFromEnd(0);
		FileHandle->Write(Bytes.GetData(), Bytes.Num());
		delete FileHandle;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to open file %s."), *FilePath);
	}
}

// Called every frame
void AAsyncTaskDownloadAsset::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	NetCheckTime += DeltaTime;

	if (bBeginDownloadPatch && !bCheckNetConnectWait)
	{
		bCheckNetConnectWait = true;
		CheckInternetConnection();
	}

	if (bBeginNextURLRequest&&IsNetworkAvailable())
	{
		bBeginNextURLRequest = false;
		RecursionDownloadUrls();
	}

	if (!bSingleFileDownloadOk && bCanChunkDownload)
	{
		bCanChunkDownload = false;
		MyHttpRequest->ProcessRequest();
	}
	
	// if (NetCheckTime >= 2.0f)
	// {
	// 	CheckInternetConnection();
	// 	NetCheckTime = 0.0f;
	// }

	//if(!IsNetworkAvailable()&& !bCancelDownload &&MyHttpRequest.IsValid())
	//{
	//	bCancelDownload=true;
	//	MyHttpRequest->CancelRequest();
	//}

	GetPatchDownloadStat(0, CurrentDownloadSize);
}

void AAsyncTaskDownloadAsset::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	GetWorld()->GetTimerManager().ClearTimer(NetTimerHandle);
}

void AAsyncTaskDownloadAsset::StartTask()
{
	if (!PakURL.IsEmpty())
	{
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

		HttpRequest->SetURL(PakURL);
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->ProcessRequest();
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &AAsyncTaskDownloadAsset::HandleRequest);
		HttpRequest->OnRequestProgress().BindUObject(this, &AAsyncTaskDownloadAsset::RequestProgress);


		OnPakDownloadOk.AddUniqueDynamic(this, &AAsyncTaskDownloadAsset::LoadPak);;
	}
}

void AAsyncTaskDownloadAsset::LoadPak()
{
	OldPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();
	PakPlatformFile = MakeShareable(new FPakPlatformFile());
	PakPlatformFile->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), TEXT(""));
	FPlatformFileManager::Get().SetPlatformFile(*PakPlatformFile.Get());


	if (FPaths::FileExists(PakName) && PakPlatformFile.IsValid())
	{
		TArray<FString> ExistPaks;
		PakPlatformFile->GetMountedPakFilenames(ExistPaks);

		if (ExistPaks.Find(PakName) >= 0)
		{
			return;
		}

		TRefCountPtr<FPakFile> TempPak = new FPakFile(PakPlatformFile.Get(), *PakName, false);
		FString OldPakMountPoint = TempPak->GetMountPoint();
		int32 ContentPos = OldPakMountPoint.Find("Content/");
		FString NewMountPath = OldPakMountPoint.RightChop(ContentPos);
		FString ProjectPath = FPaths::ProjectDir();
		NewMountPath = ProjectPath + NewMountPath;
		TempPak->SetMountPoint(*NewMountPath);

		//PAK MountPoint
		if (PakPlatformFile->Mount(*PakName, 1, *NewMountPath))
		{
			//PAK
			TArray<FString> FileList;

			TempPak->FindPrunedFilesAtPath(FileList, *TempPak->GetMountPoint(), true, false, true);
			//from PAK load StaticMesh
			for (FString& FileName : FileList)
			{
				if (FileName.EndsWith(TEXT(".uasset")))
				{
					FString NewPath = FileName;
					NewPath.RemoveFromEnd(TEXT(".uasset"));
					FString ShortName = FPackageName::GetShortName(NewPath);
					int32 Pos = NewPath.Find("/Content/");
					NewPath = NewPath.RightChop(Pos + 8);
					NewPath = "/Game" + NewPath + "." + ShortName;

					//UObject* LoadObject = StaticLoadObject(UObject::StaticClass(), NULL, *NewPath);
					FStreamableManager StreamableManager;
					FSoftObjectPath reference = NewPath;
					//load UObject
					UObject* LoadObject = StreamableManager.LoadSynchronous(reference);
					if (LoadObject && ShortName.StartsWith(TEXT("SM_")))
					{
						UStaticMesh* Mesh = Cast<UStaticMesh>(LoadObject);
						if (Mesh)
						{
							if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWorld())
							{
								UWorld* CurWorld = GEngine->GameViewport->GetWorld();
								AStaticMeshActor* MeshActor = CurWorld->SpawnActor<AStaticMeshActor>(
									AStaticMeshActor::StaticClass(), FVector(0, 0, 0), FRotator(0, 0, 0));
								if (MeshActor && MeshActor->GetStaticMeshComponent())
								{
									//StaticMesh
									MeshActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
									MeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
									GEngine->AddOnScreenDebugMessage(INDEX_NONE, 60.f, FColor::White,
									                                 MeshActor->GetName());
								}
							}
						}
					}
					else if (LoadObject && ShortName.StartsWith(TEXT("BP_")))
					{
						UBlueprint* Generate = Cast<UBlueprint>(LoadObject);
						if (Generate)
						{
							AActor* Actor = GetWorld()->SpawnActor<AActor>(Generate->GeneratedClass);
						}
					}
				}
			}
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 60.f, FColor::Red,
			                                 *FString::Printf(TEXT("Mount File Error %s %s"), *PakName, *NewMountPath));
		}
	}

	FPlatformFileManager::Get().SetPlatformFile(*OldPlatformFile);
}

void AAsyncTaskDownloadAsset::ManifestDownloadOkHandle(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse,bool bSucceeded)
{
	
	if (bSucceeded && HttpResponse.IsValid() && HttpResponse->GetContentLength() > 0)
	{
		FString JsonString = HttpResponse->GetContentAsString();
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);

		if (FJsonSerializer::Deserialize(JsonReader, JsonObject))
		{
			TSharedPtr<FJsonValue> OVersion = JsonObject->GetField<EJson::String>(TEXT("OfficialVersion"));

			//获取本地版本号
			FString LocalVersion=UWeWorldBlueprintFunctionLibrary::GetLocalVersion();

			//版本不一致处理逻辑
			OfficialVersion = OVersion->AsString();

			int32 LastDotIndex;
			OfficialVersion.FindLastChar('.', LastDotIndex);
			FString Substring = OfficialVersion.RightChop(LastDotIndex + 1);
			int32 OfficialVerNum = FCString::Atoi(*Substring);

			LocalVersion.FindLastChar('.', LastDotIndex);
			Substring = LocalVersion.RightChop(LastDotIndex + 1);
			int32 LocalVerNum = FCString::Atoi(*Substring);


			if (LocalVerNum < OfficialVerNum)
			{
				//检测本地版本号是否在官方补丁支持版本内
				bool bSupport = false;
				TArray<TSharedPtr<FJsonValue>> SupportedVersions = JsonObject->GetArrayField(TEXT("SupportedVersions"));
				for (auto ver : SupportedVersions)
				{
					FString sver = ver.Get()->AsString();
					if (sver.Equals(LocalVersion))
					{
						bSupport = true;
						break;
					}
				}

				//如果是官方支持版本，找到该版本对应的补丁列表
				if (bSupport)
				{
					// 返回值: Windows, Android,HTML5,IOS
					FString PlatformName = FPlatformProperties::IniPlatformName();
					FString PlatformVersion = PlatformName + TEXT("_") + LocalVersion;
					TArray<TSharedPtr<FJsonValue>> PatchList = JsonObject->GetArrayField(PlatformVersion);

					for (auto pl : PatchList)
					{
						FString PatchName = pl.Get()->AsString();
						TSharedPtr<FJsonValue> PatchSize = JsonObject->GetField<EJson::Number>(PatchName);

						PlatformUrlDir = BaseUrlDir + PlatformName + TEXT("/");

						FString PatchUrl = PlatformUrlDir + PatchName;
						//check patch exist
						//FString PatchFile= FPaths::ProjectSavedDir()+TEXT("Paks/")+PatchName;
						FString PatchFile=PatchDir+PatchName;
						if(!FPaths::FileExists(PatchFile))
						{
							PatchUrlList.Add(PatchUrl);
							//统计补丁size
							int64 PatchFileSize = PatchSize.Get()->AsNumber();
							FileSizeMap.Add(PatchName, PatchFileSize);
							TotalSize += PatchFileSize;
						}
					}

					//统计补丁size
					//if (bNeedDownloadBasePak)
					//{
					//	TSharedPtr<FJsonValue> BasePakSize = JsonObject->GetField<EJson::Number>(PlatformBasePakName);
					//	TotalSize += BasePakSize.Get()->AsNumber();
					//}

					OnVersionSupport(true);
					//DoPatchDownload();
				}
				else
				{
					OnVersionSupport(false);
				}
			}
			else
			{
				OnVersionSupport(false);
			}
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1,60.0f,FColor::Red,TEXT("ERROR::服务器热更新配置清单格式错误，请检查！"));
			OnVersionSupport(false);
		}

		//OnPakDownloadOk.Broadcast(FilePath);
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 60.0f, FColor::Red, TEXT("ERROR::CheckVersion 服务区请求超时，请检查网络后重启程序！"));
		OnVersionSupport(false);
	}
}

void AAsyncTaskDownloadAsset::ManifestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
{
	GetManifestDownloadStat(BytesSent, BytesReceived);
}

void AAsyncTaskDownloadAsset::PatchDownloadOkHandle(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse,
                                                    bool bSucceeded)
{
	int32 ContentSize = HttpResponse->GetContentLength();
	if (bSucceeded && HttpResponse.IsValid() && ContentSize > 0)
	{
		FileStepDownloadSize += ContentSize;
		CurrentDownloadSize += FileStepDownloadSize;

		//FString FilePath = FPaths::ProjectSavedDir() + TEXT("Paks/") + CurrentPatchName;
		FString FilePath=PatchDir+CurrentPatchName;

		FFileHelper::SaveArrayToFile(HttpResponse->GetContent(), *FilePath);

		//检测是否所有补丁下载完成
		if (FileStepDownloadSize >= TotalSize)
		{
			if (FCoreDelegates::OnMountAllPakFiles.IsBound())
			{
				TArray<FString> PakFolders;
				//PakFolders.Add(FPaths::ProjectSavedDir() + TEXT("Paks/"));
				PakFolders.Add(PatchDir);
				FCoreDelegates::OnMountAllPakFiles.Execute(PakFolders);
			}

			OnPatchDownloadComplete();

			//将官方版本号写入配置
			//GConfig->SetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),
			//                   TEXT("ProjectVersion"),
			//                   *OfficialVersion,
			//                   GGameIni);
			//GConfig->Flush(true, GGameIni);

			//FString FileName = FPaths::ProjectSavedDir() + TEXT("/Paks/Version.info");
			FString FileName=PatchDir + TEXT("Version.info");
			FFileHelper::SaveStringToFile(OfficialVersion, *FileName);

			bBeginDownloadPatch = false;
		}

		bBeginNextURLRequest = true;
	}
}

void AAsyncTaskDownloadAsset::PatchDownloadProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
{
	GetPatchDownloadStat(BytesSent, CurrentDownloadSize + BytesReceived);
}

void AAsyncTaskDownloadAsset::HandleRequest(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	int32 ContentSize = HttpResponse->GetContentLength();
	if (bSucceeded && HttpResponse.IsValid() && ContentSize > 0)
	{
		FString FilePath;
		if (PakStoragePath.EndsWith(TEXT("/")) || PakStoragePath.EndsWith(TEXT("\\")))
		{
			FilePath = PakStoragePath + PakName;
		}
		else
		{
			PakStoragePath += TEXT("/");
			FilePath = PakStoragePath + PakName;
		}

		FFileHelper::SaveArrayToFile(HttpResponse->GetContent(), *FilePath);

		OnPakDownloadOk.Broadcast();
	}
}

void AAsyncTaskDownloadAsset::RequestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
{
}

void AAsyncTaskDownloadAsset::CheckGameVersion()
{
	//检查基础包是否完整
	//for (const auto& ResName : BasePakNameList)
	//{
	//	int32 index;
	//	ResName.FindLastChar('/', index);
	//	FString FileName = ResName.Right(ResName.Len() - index - 1);
	//	bNeedDownloadBasePak = !FPaths::FileExists(FPaths::ProjectContentDir() + TEXT("Paks/") + FileName);
	//}

	// FString PlatformName = FPlatformProperties::IniPlatformName();
	// if (PlatformName == TEXT("Android")||PlatformName==TEXT("IOS"))
	// {
	// 	bNeedDownloadBasePak = true;
	// }

	//从服务器下载manifest做版本比对  http://127.0.0.1/FilePakTestCDN/PatchManifest.json 
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();


	HttpRequest->SetURL(BaseUrlDir + TEXT("PatchManifest.json"));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetTimeout(5.0f);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &AAsyncTaskDownloadAsset::ManifestDownloadOkHandle);
	HttpRequest->OnRequestProgress().BindUObject(this, &AAsyncTaskDownloadAsset::ManifestProgress);
	HttpRequest->ProcessRequest();
}

void AAsyncTaskDownloadAsset::DoPatchDownload()
{
	//FBackgroundHttpRequestPtr BHttpRequest = FBackgroundHttpModule::Get().CreateBackgroundRequest();
	bBeginDownloadPatch = true;
	FString PlatformName = FPlatformProperties::IniPlatformName();
	PlatformUrlDir = BaseUrlDir + PlatformName + TEXT("/");

	for (auto url : PatchUrlList)
	{
		Urls.Add(url);
		// #if PLATFORM_IOS
		// 		FString ucasFile = FileName + TEXT(".ucas");
		// 		FString utocFile = FileName + TEXT(".utoc");
		// 		Urls.Add(PlatformUrlDir + ucasFile);
		// 		Urls.Add(PlatformUrlDir + utocFile);
		// 		NeedDownloadFileNames.Add(ucasFile);
		// 		NeedDownloadFileNames.Add(utocFile);
		// #endif

		NeedDownloadFileNames.Add(FPaths::GetCleanFilename(url));

		//if (bNeedDownloadBasePak)
		//{
		//	for (const auto& ResName : BasePakNameList)
		//	{
		//		Urls.Add(PlatformUrlDir + ResName);
		//		NeedDownloadFileNames.Add(ResName);
		//	}
		//	bNeedDownloadBasePak = true;
		//}
	}
	
	bBeginNextURLRequest = true;
}

void AAsyncTaskDownloadAsset::UnitTest()
{
	// FString url = BaseUrlDir+TEXT("/Android/Hot-Android_ETC2_0_P.pak");
	//
	// TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	// int32 index;
	// url.FindLastChar('/', index);
	// CurrentPatchName = url.Right(url.Len() - index - 1);
	// HttpRequest->SetURL(url);
	// HttpRequest->SetVerb(TEXT("GET"));
	// HttpRequest->ProcessRequest();
	// HttpRequest->OnProcessRequestComplete().BindUObject(this, &AAsyncTaskDownloadAsset::PatchDownloadOkHandle);
	// HttpRequest->OnRequestProgress().BindUObject(this, &AAsyncTaskDownloadAsset::PatchDownloadProgress);


	OldPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();
	PakPlatformFile = MakeShareable(new FPakPlatformFile());
	PakPlatformFile->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), TEXT(""));
	FPlatformFileManager::Get().SetPlatformFile(*PakPlatformFile.Get());

	FString pakFile = FPaths::ProjectDir() + TEXT("Paks/hotup-ios_0_p.pak");
	if (FPaths::FileExists(pakFile) && PakPlatformFile.IsValid())
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Red, TEXT("hotup-ios_0_p.pak File Exists"));
		TArray<FString> ExistPaks;
		PakPlatformFile->GetMountedPakFilenames(ExistPaks);

		if (ExistPaks.Find(pakFile) >= 0)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Red, TEXT("hotup-ios_0_p.pak Has Mount!"));
			return;
		}

		TRefCountPtr<FPakFile> TempPak = new FPakFile(PakPlatformFile.Get(), *pakFile, false);
		FString OldPakMountPoint = TempPak->GetMountPoint();
		int32 ContentPos = OldPakMountPoint.Find("Paks/");
		FString NewMountPath = OldPakMountPoint.RightChop(ContentPos);
		FString ProjectPath = FPaths::ProjectDir();
		NewMountPath = ProjectPath + NewMountPath;
		TempPak->SetMountPoint(*NewMountPath);

		//PAK MountPoint
		if (PakPlatformFile->Mount(*pakFile, 1, *NewMountPath))
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Red, TEXT("hotup-ios_0_p.pak Mount OK!"));
			//PAK
			TArray<FString> FileList;

			TempPak->FindPrunedFilesAtPath(FileList, *TempPak->GetMountPoint(), true, false, true);
			//from PAK load StaticMesh
			for (FString& FileName : FileList)
			{
				if (FileName.EndsWith(TEXT(".uasset")))
				{
					FString NewPath = FileName;
					NewPath.RemoveFromEnd(TEXT(".uasset"));
					FString ShortName = FPackageName::GetShortName(NewPath);
					int32 Pos = NewPath.Find("/Content/");
					NewPath = NewPath.RightChop(Pos + 8);
					NewPath = "/Game" + NewPath + "." + ShortName;

					//UObject* LoadObject = StaticLoadObject(UObject::StaticClass(), NULL, *NewPath);
					FStreamableManager StreamableManager;
					FSoftObjectPath reference = NewPath;
					//load UObject
					UObject* LoadObject = StreamableManager.LoadSynchronous(reference);
					if (LoadObject && ShortName.StartsWith(TEXT("Con")))
					{
						UStaticMesh* Mesh = Cast<UStaticMesh>(LoadObject);
						if (Mesh)
						{
							if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWorld())
							{
								UWorld* CurWorld = GEngine->GameViewport->GetWorld();
								AStaticMeshActor* MeshActor = CurWorld->SpawnActor<AStaticMeshActor>(
									AStaticMeshActor::StaticClass(), FVector(0, 0, 0), FRotator(0, 0, 0));
								if (MeshActor && MeshActor->GetStaticMeshComponent())
								{
									//StaticMesh
									MeshActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
									MeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
									GEngine->AddOnScreenDebugMessage(INDEX_NONE, 60.f, FColor::White,
									                                 MeshActor->GetName());
								}
							}
						}
					}
					else if (LoadObject && ShortName.StartsWith(TEXT("bp")))
					{
						UBlueprint* Generate = Cast<UBlueprint>(LoadObject);
						if (Generate)
						{
							AActor* Actor = GetWorld()->SpawnActor<AActor>(Generate->GeneratedClass);
						}
					}
				}
			}
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 60.f, FColor::Red,
			                                 *FString::Printf(TEXT("Mount File Error %s %s"), *PakName, *NewMountPath));
		}
	}

	FPlatformFileManager::Get().SetPlatformFile(*OldPlatformFile);
}

bool AAsyncTaskDownloadAsset::IsNetworkAvailable()
{
	//if (GetWorld())
	//{
	//	if (GetWorld()->GetNetDriver())
	//	{
	//		UNetConnection* Connection = GetWorld()->GetNetDriver()->ServerConnection;
	//		return (Connection != nullptr) && (Connection->State == USOCK_Open);
	//	}
	//}

	//return false;
	return bNetConnect;
}

void AAsyncTaskDownloadAsset::DownloadFile(const FString& URL, const FString& FileSavePath, int32 FileTotalSize,int32 ChunkSize)
{
	FString FileName = FPaths::GetCleanFilename(URL);
	FString TempName = FileName + TEXT(".temp");

	//FString TempFilePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Paks"), TempName);
	FString TempFilePath =PatchDir+TempName;
	
	FString DirectoryPath = FPaths::GetPath(FileSavePath);

	if (!FPaths::DirectoryExists(DirectoryPath))
	{
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*DirectoryPath);
	}

	MyHttpRequest = FHttpModule::Get().CreateRequest();
	MyHttpRequest->SetURL(URL);
	MyHttpRequest->SetVerb(TEXT("GET"));
	MyHttpRequest->SetTimeout(5.0f);
	if (FPaths::FileExists(TempFilePath))
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const int64 TempFileSize = PlatformFile.FileSize(*TempFilePath);
		CurrentDownloadSize = TempFileSize + FileStepDownloadSize;

		if (CurrentDownloadSize >= TotalSize)
		{
			//const FString FileSavePath = FPaths::ProjectSavedDir() + TEXT("Paks/") + CurrentPatchName;
			const FString FileSavePatch =	PatchDir+CurrentPatchName;
			PlatformFile.MoveFile(*FileSavePath, *TempFilePath);
			if (FPaths::FileExists(TempFilePath))
			{
				PlatformFile.DeleteFile(*TempFilePath);
			}
		}
		const int32 DeltaSize = FileTotalSize - TempFileSize;
		FString Size;
		if (DeltaSize >= ChunkSize)
		{
			Size = FString::Printf(TEXT("bytes=%lld-%lld"), TempFileSize, TempFileSize + ChunkSize - 1);
		}
		else
		{
			Size = FString::Printf(TEXT("bytes=%lld-%lld"), TempFileSize, TempFileSize + DeltaSize - 1);
		}

		MyHttpRequest->SetHeader(TEXT("Range"), Size);
	}
	else
	{
		CurrentDownloadSize = FileStepDownloadSize;
		FString Size = FString::Printf(TEXT("bytes=%ld-%ld"), 0, ChunkSize - 1);
		MyHttpRequest->SetHeader(TEXT("Range"), Size);
	}

	MyHttpRequest->OnProcessRequestComplete().BindLambda(
		[TempFilePath, FileTotalSize, FileSavePath, ChunkSize, this](FHttpRequestPtr Request, FHttpResponsePtr Response,
		                                                             bool bSuccess)
		{
			int32 FileSize = Response->GetContentLength();
			if(FileSize == 468)
			{
				GEngine->AddOnScreenDebugMessage(-1,10.0f,FColor::Red,Response->GetContentAsString());
			}
			if (bSuccess && Response.IsValid() && FileSize > 0)
			{
				CurrentDownloadSize += Response->GetContentLength();
				if (FPaths::FileExists(TempFilePath))
				{ 
					AppendBytesToFile(TempFilePath, Response->GetContent());
				}
				else
				{
					FFileHelper::SaveArrayToFile(Response->GetContent(), *TempFilePath);
				}

				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				int64 TempFileSize = PlatformFile.FileSize(*TempFilePath);

				if (TempFileSize >= FileTotalSize)
				{
					bSingleFileDownloadOk = true;
					bCanChunkDownload = false;
					bBeginNextURLRequest = true;
					
					PlatformFile.MoveFile(*FileSavePath, *TempFilePath);

					while (!FPaths::FileExists(FileSavePath))
					{
						FPlatformProcess::Sleep(0.1f);
					}

					FileStepDownloadSize += TempFileSize;
					//检测是否所有补丁下载完成
					if (FileStepDownloadSize >= TotalSize)
					{

						if (FCoreDelegates::OnMountAllPakFiles.IsBound())
						{
							TArray<FString> PakFolders;
							//PakFolders.Add(FPaths::ProjectSavedDir() + TEXT("Paks/"));
							PakFolders.Add(PatchDir);
							FCoreDelegates::OnMountAllPakFiles.Execute(PakFolders);
						}

						//将官方版本号写入配置
						//GConfig->SetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),
						//                   TEXT("ProjectVersion"),
						//                   *OfficialVersion,
						//                   GGameIni);
						//GConfig->Flush(true, GGameIni);

						//FString FileName = FPaths::ProjectSavedDir() + TEXT("/Paks/Version.info");
						FString FileName = PatchDir+TEXT("Version.info");
						FFileHelper::SaveStringToFile(OfficialVersion, *FileName);

						OnPatchDownloadComplete();

						bBeginDownloadPatch = false;
					}
				}
				const int32 DeltaSize = FileTotalSize - TempFileSize;
				FString Size;
				if (DeltaSize >= ChunkSize)
				{
					Size = FString::Printf(TEXT("bytes=%lld-%lld"), TempFileSize, TempFileSize + ChunkSize - 1);
				}
				else
				{
					Size = FString::Printf(TEXT("bytes=%lld-%lld"), TempFileSize, TempFileSize + DeltaSize - 1);
				}

				MyHttpRequest->SetHeader(TEXT("Range"), Size);
				if (!bSingleFileDownloadOk)
				{
					bCanChunkDownload = true;
				}
				
			}
			else
			{
				if(bSingleFileDownloadOk)
				{
					bSingleFileDownloadOk=false;
					return;
				}
				//Reset Download property , Retry Download
				bCanChunkDownload = false;
				bSingleFileDownloadOk=false;
				bBeginNextURLRequest=false;
				bNetConnect = false;
				ResumeUrl = PlatformUrlDir + CurrentPatchName;
				if(Urls.Find(ResumeUrl) == INDEX_NONE)
				{
					Urls.Insert(ResumeUrl, 0);
				}
			}
		});

	//MyHttpRequest->OnRequestProgress().BindLambda([this](FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
	//{
	//	GetPatchDownloadStat(BytesSent, CurrentDownloadSize);
	//});
	MyHttpRequest->ProcessRequest();
}

void AAsyncTaskDownloadAsset::BeginUnitTest()
{
	FString TestUrl = TEXT("http://43.136.71.116/NewPak/TestDownloads.rar");
	//ChunkSize=1MB
	DownloadFile(TestUrl, TEXT("D:/Test/111.rar"), 207132580, 1048576);
}

void AAsyncTaskDownloadAsset::EndUnitTest()
{
	MyHttpRequest->CancelRequest();
}

void AAsyncTaskDownloadAsset::CleanupPatchCache()
{
	FString LocaVersion= UWeWorldBlueprintFunctionLibrary::GetLocalVersion();
	TArray<FString> LocalArray;
	LocaVersion.ParseIntoArray(LocalArray, TEXT("."), true);
	if(LocalArray.Num()!=4)
	{
		GEngine->AddOnScreenDebugMessage(-1,10.0f,FColor::Red,TEXT("Local版本号位数配置错误：请检查Game.ini配置"));
		return;
	}
	int LocalMainVerion = FCString::Atoi(*LocalArray[2]);
	
	//FString FileName = FPaths::ProjectSavedDir() + TEXT("/Paks/Version.info");
	FString FileName=PatchDir+TEXT("Version.info");
	FString HotUpdateVersion=LocaVersion;
	if(FPaths::FileExists(FileName))
	{
		//open file
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.FileExists(*FileName))
		{
			IFileHandle* FileHandle = PlatformFile.OpenRead(*FileName);
			if (FileHandle)
			{
				// read file
				TArray<uint8> FileData;
				FileData.AddUninitialized(FileHandle->Size());
				FileHandle->Read(FileData.GetData(),FileHandle->Size());

				// convert to string
				HotUpdateVersion = FString(UTF8_TO_TCHAR(FileData.GetData()));
			}
		}
	}
	
	TArray<FString> HotArray;
	HotUpdateVersion.ParseIntoArray(HotArray, TEXT("."), true);
	if(HotArray.Num()!=4)
	{
		GEngine->AddOnScreenDebugMessage(-1,10.0f,FColor::Red,TEXT("HotUpdate版本号位数配置错误：请检查version.info配置"));
		return;
	}
	int HotUpdateMainVerion = FCString::Atoi(*HotArray[2]);

	if(HotUpdateMainVerion<LocalMainVerion)
	{
		DeleteDirectoryRecursively(PatchDir);
		//DeleteDirectoryRecursively(FPaths::ProjectSavedDir()+TEXT("Paks/"));
	}
}

void AAsyncTaskDownloadAsset::DeleteDirectoryRecursively(const FString& InDirectory)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString Directory = InDirectory;
	FPaths::NormalizeDirectoryName(Directory);

	// Recursively delete all files and directories inside the directory
	FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*Directory, [this](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
	{
		FString FilePath = FString(FilenameOrDirectory);
		if (bIsDirectory)
		{
			// If the file is a directory, recursively call this function to delete all its contents
			DeleteDirectoryRecursively(FilePath);
		}
		else
		{
			// If the file is a file, delete it
			FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*FilePath);
		}
		return true;
	});

	// Delete the directory itself
	FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*Directory);
}

void AAsyncTaskDownloadAsset::RecursionDownloadUrls()
{
	if (Urls.IsEmpty())
		return;

	if(bSingleFileDownloadOk)
	{
		MyHttpRequest->ClearTimeout();
		MyHttpRequest->CancelRequest();
	}
	bSingleFileDownloadOk = false;
	FString URL = Urls[0];
	Urls.RemoveAt(0);
	CurrentPatchName = FPaths::GetCleanFilename(URL);

	const FString FileSavePath = PatchDir + CurrentPatchName;

	

// #if PLATFORM_IOS
//  	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
//  	HttpRequest->SetURL(URL);
//  	HttpRequest->SetVerb(TEXT("GET"));
//  	//HttpRequest->SetTimeout(30.0f);
//  	HttpRequest->OnProcessRequestComplete().BindUObject(this, &AAsyncTaskDownloadAsset::PatchDownloadOkHandle);
//  	HttpRequest->OnRequestProgress().BindUObject(this, &AAsyncTaskDownloadAsset::PatchDownloadProgress);
//  	HttpRequest->ProcessRequest();
// #else
	DownloadFile(URL, FileSavePath, FileSizeMap[CurrentPatchName], 1048576);
//#endif
}

void AAsyncTaskDownloadAsset::ResumeDownload()
{
	bCancelDownload=false;
	bBeginNextURLRequest = true;
}

void AAsyncTaskDownloadAsset::CheckInternetConnection()
{
	TSharedRef<IHttpRequest> MyNetCheckRequest = FHttpModule::Get().CreateRequest();
	// MyNetCheckRequest->OnProcessRequestComplete().BindLambda(
	// 	[this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	// 	{
	// 		if (bSuccess && Response.IsValid() && Response->GetResponseCode() == EHttpResponseCodes::Ok)
	// 		{
	// 			// 网络连接正常
	// 			bNetConnect = true;
	// 		}
	// 		else
	// 		{
	// 			// 网络连接失败
	// 			bNetConnect = false;
	// 		}
	// 	});
	
	MyNetCheckRequest->OnProcessRequestComplete().BindUObject(this,&AAsyncTaskDownloadAsset::NetCheckHandle);
	MyNetCheckRequest->SetURL(TEXT("https://www.zhihu.com"));
	MyNetCheckRequest->SetVerb(TEXT("GET"));
	MyNetCheckRequest->SetTimeout(5.0f);
	MyNetCheckRequest->ProcessRequest();
	
	if (!bNetConnect)
	{
		OnNetBreak();
	}
}

void AAsyncTaskDownloadAsset::NetCheckHandle(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse,bool bSucceeded)
{
	if (bSucceeded && HttpResponse.IsValid() && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok)
	{
		// 网络连接正常
		bNetConnect = true;
		bCheckNetConnectWait = false;
	}
	else
	{
		// 网络连接失败
		bNetConnect = false;
		bCheckNetConnectWait = false;
	}
}
