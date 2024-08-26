#include "WebSInt.h"
#include "WebSocketsModule.h" // Module definition
#include "IWebSocket.h"       // Socket definition
#include "Misc/Base64.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

// Sets default values
AWebSInt::AWebSInt()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AWebSInt::BeginPlay()
{
    Super::BeginPlay();

    // Initialize WebSocket
    WebSocket = FWebSocketsModule::Get().CreateWebSocket(TEXT("ws://10.10.35.1:8765"));

    if (WebSocket.IsValid())
    {
        // Capture 'this' to access class members inside the lambda
        WebSocket->OnConnected().AddLambda([this]() {
            UE_LOG(LogTemp, Log, TEXT("WebSocket connected!"));

            // Use 'this' to access the WebSocket member variable
            WebSocket->Send(TEXT("ping"));
            });

        WebSocket->OnConnectionError().AddLambda([](const FString& Error) {
            UE_LOG(LogTemp, Error, TEXT("WebSocket connection error: %s"), *Error);
            });

        WebSocket->OnClosed().AddLambda([](int32 StatusCode, const FString& Reason, bool bWasClean) {
            UE_LOG(LogTemp, Log, TEXT("WebSocket closed. Status: %d Reason: %s"), StatusCode, *Reason);
            });

        WebSocket->OnMessage().AddLambda([this](const FString& Message) {
            this->OnMessageReceived(Message);
            });

        WebSocket->OnMessageSent().AddLambda([](const FString& MessageString) {
            UE_LOG(LogTemp, Log, TEXT("WebSocket message sent: %s"), *MessageString);
            });

        // Connect to WebSocket server
        WebSocket->Connect();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create WebSocket!"));
    }
}

// Called every frame
void AWebSInt::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AWebSInt::OnMessageReceived(const FString& Message)
{
    UE_LOG(LogTemp, Log, TEXT("OnMessageReceived: Received message: %s"), *Message);

    // Parse the JSON response
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("OnMessageReceived: JSON successfully parsed."));

        FString MessageType;
        if (JsonObject->TryGetStringField("type", MessageType))
        {
            UE_LOG(LogTemp, Log, TEXT("OnMessageReceived: Message type is %s"), *MessageType);

            if (MessageType == "end_text")
            {
                // Parse the content into separate lines
                TArray<FString> Lines;
                TextData[0].ParseIntoArrayLines(Lines);

                // Add each line to the TextData array
                for (const FString& Line : Lines)
                {
                    DataStruct.TextData.Add(Line);
                }

                UE_LOG(LogTemp, Log, TEXT("OnMessageReceived: end_text received, text data stored. Number of lines: %d"), DataStruct.TextData.Num());
            }
            else if (MessageType == "end_image")
            {
                // Image processing done
                UE_LOG(LogTemp, Log, TEXT("OnMessageReceived: end_image received, processing image."));
                OnImageReceived();
            }
            else if (MessageType == "end_message")
            {
                // Broadcast the final data
                UE_LOG(LogTemp, Log, TEXT("OnMessageReceived: Final message received, broadcasting results."));
                OnDataResultsReady.Broadcast(DataStruct);
            }
            else
            {
                // Process regular data messages (text, image chunks, etc.)
                FString Content;
                if (JsonObject->TryGetStringField("content", Content))
                {
                    if (MessageType == "image_chunk")
                    {
                        UE_LOG(LogTemp, Log, TEXT("OnMessageReceived: Processing image chunk."));

                        // Assemble the image chunks here
                        TArray<uint8> DecodedImage;
                        FBase64::Decode(Content, DecodedImage);

                        // Append to the full image data until end_image is received
                        ImageDataArray.Append(DecodedImage);
                        UE_LOG(LogTemp, Log, TEXT("OnMessageReceived: Image chunk appended."));
                    }
                    else if (MessageType == "text")
                    {
                        // Handle text data
                        TextData.Add(Content);
                        UE_LOG(LogTemp, Log, TEXT("OnMessageReceived: Text data received and added."));
                    }
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("OnMessageReceived: Message does not contain a 'type' field."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("OnMessageReceived: Failed to parse JSON."));
    }
}

void AWebSInt::OnImageReceived()
{
    UE_LOG(LogTemp, Log, TEXT("OnImageReceived: Processing complete image data."));

    // Assuming ImageDataArray contains the complete image data (after receiving all chunks)

    // Step 1: Create the texture from the decoded image data
    UTexture2D* CreatedTexture = CreateTextureFromDecodedImageData(ImageDataArray, EImageFormat::PNG);

    if (CreatedTexture)
    {
        UE_LOG(LogTemp, Log, TEXT("OnImageReceived: Texture created successfully."));

        // Step 2: Add the created texture to the DataStruct Textures array
        DataStruct.Textures.Add(CreatedTexture);

        // Step 3: Optionally save the texture to a file (as you did previously)
        FString Filename = FString::Printf(TEXT("image_%d.png"), DataStruct.Textures.Num());
        FString FullPath = FPaths::ProjectSavedDir() + Filename;
        FFileHelper::SaveArrayToFile(ImageDataArray, *FullPath);

        UE_LOG(LogTemp, Log, TEXT("OnImageReceived: Texture saved to %s"), *Filename);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("OnImageReceived: Failed to create texture."));
    }

    // Step 4: Clear the ImageDataArray for the next image
    ImageDataArray.Empty();
    UE_LOG(LogTemp, Log, TEXT("OnImageReceived: ImageDataArray cleared."));
}


UTexture2D* AWebSInt::CreateTextureFromDecodedImageData(const TArray<uint8>& DecodedImageData, EImageFormat ImageFormat)
{
    UE_LOG(LogTemp, Log, TEXT("CreateTextureFromDecodedImageData: Starting texture creation process."));
    UE_LOG(LogTemp, Log, TEXT("CreateTextureFromDecodedImageData: DecodedImageData size: %d bytes"), DecodedImageData.Num());
    UE_LOG(LogTemp, Log, TEXT("CreateTextureFromDecodedImageData: ImageFormat: %d"), (int32)ImageFormat);

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

    if (ImageWrapper.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("CreateTextureFromDecodedImageData: ImageWrapper is valid."));

        if (ImageWrapper->SetCompressed(DecodedImageData.GetData(), DecodedImageData.Num()))
        {
            UE_LOG(LogTemp, Log, TEXT("CreateTextureFromDecodedImageData: Compressed data set successfully."));

            TArray<uint8> UncompressedBGRA;
            if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
            {
                UTexture2D* Texture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);
                if (!Texture)
                {
                    UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDecodedImageData: Failed to create transient texture."));
                    return nullptr;
                }

                void* TextureData = Texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
                FMemory::Memcpy(TextureData, UncompressedBGRA.GetData(), UncompressedBGRA.Num());
                Texture->PlatformData->Mips[0].BulkData.Unlock();

                Texture->UpdateResource();
                UE_LOG(LogTemp, Log, TEXT("CreateTextureFromDecodedImageData: Texture resource updated."));

                if (Texture)
                {
                    DataStruct.Textures.Add(Texture);
                    UE_LOG(LogTemp, Log, TEXT("CreateTextureFromDecodedImageData: Texture added to DataStruct."));
                }

                return Texture;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("CreateTextureFromDecodedImageData: Failed to get raw image data."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDecodedImageData: Failed to set compressed data. Check if the image format and data are correct."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDecodedImageData: ImageWrapper is invalid. This might be due to an unsupported image format."));
    }

    return nullptr;
}
