#pragma once

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <android/asset_manager_jni.h>

#include "types.h"
#include "Memory.h"

class FileManager {


public:
    static inline AAssetManager *assetManager;

    class AssetBuffer {
    private:
        friend FileManager;

        static inline
        uint32 AllocSize(uint32 bytes) { return sizeof(AssetBuffer) + bytes; }

    public:
        uint64 size;
        uint8 data[1]; //Note: data is null terminated
    };

    static constexpr uint32 kMaxAssetBytes = MaxUint32() - sizeof(AssetBuffer);


    static void Init(JNIEnv *env, jobject jAssetManager) {
        RUNTIME_ASSERT(!assetManager,
                       "AssetManager already Initialized { requested jAssetManger: %p, assetManager: %p }",
                       jAssetManager, assetManager);

        assetManager = AAssetManager_fromJava(env, jAssetManager);
        RUNTIME_ASSERT(assetManager, "Failed to Initialize { requested jAssetManger: %p }",
                       jAssetManager);

        Log("Initialed assetManger { jAssetManger: %p, assetManager: %p }", jAssetManager,
            assetManager);
    }

    static AssetBuffer *OpenAsset(const char *assetPath, Memory::Arena *arena) {

        AAsset *asset = AAssetManager_open(assetManager, assetPath, AASSET_MODE_BUFFER);
        RUNTIME_ASSERT(asset, "Failed to open asset { assetManger: %p, assetPath: %s }",
                       assetManager, assetPath);

        off_t bytes = AAsset_getLength(asset);
        RUNTIME_ASSERT(bytes <= kMaxAssetBytes,
                       "Asset exceeds maximum size { assetPath: %s, bytes: %lu, kMaxAssetBytes: u% }",
                       assetPath, bytes, kMaxAssetBytes);

        AssetBuffer *buffer = (AssetBuffer *) arena->PushBytes(AssetBuffer::AllocSize(bytes),
                                                               false);
        buffer->size = bytes;

        // Keep reading asset until everything is full
        uint8 *dataEnd = (uint8 *) ByteOffset(buffer->data, bytes);
        while (bytes > 0) {

            int32 readSize = AAsset_read(asset, ByteOffset(dataEnd, -bytes), bytes);
            RUNTIME_ASSERT(readSize > 0,
                           "Failed To Read Asset { asset: %d, assetPath: %s, assetBytes: %d, position: %d, error: %d }",
                           asset, assetPath, buffer->size, bytes, readSize
            );

            bytes -= readSize;
        }
        AAsset_close(asset);

        *dataEnd = 0; //null terminate data - Note: 'AssetBuffer::AllocSize' reserves 1 extra byte for trailing null
        return buffer;
    }

    //TODO: TEST THIS
    static void SaveAsFile(AssetBuffer *buffer, const char *filePath) {
        RUNTIME_ASSERT(buffer, "Null Buffer");
        RUNTIME_ASSERT(filePath, "Null FilePath");

        int fd = open(filePath, O_CREAT | O_WRONLY, S_IWRITE | S_IWGRP);
        RUNTIME_ASSERT(fd >= 0,
                       "Failed to open file for writing { assetBuffer: %p, filePath: %s, linux errono: %d }",
                       buffer, filePath, errno);

        size_t bytesWritten = write(fd, buffer->data, buffer->size);
        RUNTIME_ASSERT(bytesWritten >= 0,
                       "Failed to write buffer to file { assetBuffer: %p, filePath: %s, linux errono: %d}",
                       buffer, filePath, errno);

        RUNTIME_ASSERT(bytesWritten == buffer->size,
                       "Wrote different number of bytes to file { assetBuffer: %p, filePath: %s, bufferSize: %d, bytesWritten: %d}",
                       buffer, filePath, buffer->size, bytesWritten);
    }

    //static void AssetToFile(const AssetBuffer* file, const char* filePath) {
    //      static constexpr uint32 kAssetBufferSize = MB(1);
    //      static AAssetManager* assetManager;
    //      static Memory::Arena memoryArena;
    //    //TODO: SAVE TO FILE WITHOUT USING INTERMEDIATE BUFFER!
    //}

};