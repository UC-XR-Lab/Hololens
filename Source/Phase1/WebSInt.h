// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IWebSocket.h"
#include "TextureResource.h"
#include "IImageWrapperModule.h"
#include "Engine/Texture2D.h"
#include "WebSInt.generated.h"

USTRUCT(BlueprintType)
struct FDataRetrieved
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<UTexture2D*> Textures;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> TextData;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDataResultsReady, const FDataRetrieved&, DataStructure);

UCLASS()
class PHASE1_API AWebSInt : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWebSInt();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Exposing an array of strings to Blueprints
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "WebSocketInt")
	TArray<FString> TextData;

	// Data structure for storing collected data
	FDataRetrieved DataStruct;

	// Delegate to signal when all data is received
	UPROPERTY(BlueprintAssignable, Category = "WebSocketInt")
	FOnDataResultsReady OnDataResultsReady;

private:

	TSharedPtr<IWebSocket> WebSocket;

	// Buffer to hold image data
	TArray<uint8> ImageDataArray;

	void OnConnected();
	void OnConnectionError(const FString& Error);
	void OnMessageReceived(const FString& Message);
	void OnMessageSent();
	void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	UTexture2D* CreateTextureFromDecodedImageData(const TArray<uint8>& DecodedImageData, EImageFormat ImageFormat);

	// Function to handle when image reception is complete
	void OnImageReceived();
};
