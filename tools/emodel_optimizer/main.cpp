#include <meshoptimizer.h>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    uint32_t vertexCount, vertexStride, indexCount;
    fread(&vertexCount, sizeof(uint32_t), 1, stdin);
    fread(&vertexStride, sizeof(uint32_t), 1, stdin);
    fread(&indexCount, sizeof(uint32_t), 1, stdin);

    std::vector<uint8_t> vertices(vertexCount * vertexStride);
    fread(vertices.data(), 1, vertices.size(), stdin);

    std::vector<uint32_t> indices(indexCount);
    fread(indices.data(), sizeof(uint32_t), indexCount, stdin);

    std::vector<uint32_t> remap(vertexCount);
    size_t uniqueVertexCount = meshopt_generateVertexRemap(
        remap.data(), indices.data(), indexCount,
        vertices.data(), vertexCount, vertexStride);

    std::vector<uint8_t> remappedVertices(uniqueVertexCount * vertexStride);
    std::vector<uint32_t> remappedIndices(indexCount);

    meshopt_remapVertexBuffer(remappedVertices.data(), vertices.data(),
        vertexCount, vertexStride, remap.data());
    meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(),
        indexCount, remap.data());

    meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(),
        indexCount, uniqueVertexCount);

    meshopt_optimizeOverdraw(remappedIndices.data(), remappedIndices.data(),
        indexCount, (const float*)remappedVertices.data(),
        uniqueVertexCount, vertexStride, 1.05f);

    std::vector<uint8_t> finalVertices(uniqueVertexCount * vertexStride);
    meshopt_optimizeVertexFetch(finalVertices.data(), remappedIndices.data(),
        indexCount, remappedVertices.data(), uniqueVertexCount, vertexStride);

    std::vector<uint8_t> compressedVertices(meshopt_encodeVertexBufferBound(uniqueVertexCount, vertexStride));
    size_t compressedVertexSize = meshopt_encodeVertexBuffer(
        compressedVertices.data(), compressedVertices.size(),
        finalVertices.data(), uniqueVertexCount, vertexStride);

    std::vector<uint8_t> compressedIndices(meshopt_encodeIndexBufferBound(indexCount, uniqueVertexCount));
    size_t compressedIndexSize = meshopt_encodeIndexBuffer(
        compressedIndices.data(), compressedIndices.size(),
        remappedIndices.data(), indexCount);

    uint32_t outVertexCount = (uint32_t)uniqueVertexCount;
    uint32_t outCompVertexSize = (uint32_t)compressedVertexSize;
    uint32_t outIndexCount = (uint32_t)indexCount;
    uint32_t outCompIndexSize = (uint32_t)compressedIndexSize;

    fwrite(&outVertexCount, sizeof(uint32_t), 1, stdout);
    fwrite(&outCompVertexSize, sizeof(uint32_t), 1, stdout);
    fwrite(&outIndexCount, sizeof(uint32_t), 1, stdout);
    fwrite(&outCompIndexSize, sizeof(uint32_t), 1, stdout);
    fwrite(compressedVertices.data(), 1, compressedVertexSize, stdout);
    fwrite(compressedIndices.data(), 1, compressedIndexSize, stdout);

    return 0;
}