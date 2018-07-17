/**
MIT License

Copyright (c) 2018 NVIDIA CORPORATION. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*
*/

#include "calibrator.h"
#include <fstream>
#include <iostream>
#include <iterator>

Int8EntropyCalibrator::Int8EntropyCalibrator(const int& batchSize,
                                             const std::string& calibrationSetPath,
                                             const std::string& calibTableFilePath,
                                             const uint64_t& inputSize, const int& inputH,
                                             const int& inputW, const std::string& inputBlobName) :
    m_BatchSize(batchSize),
    m_InputH(inputH),
    m_InputW(inputW),
    m_InputSize(inputSize),
    m_InputCount(m_BatchSize * m_InputSize),
    m_InputBlobName(inputBlobName.c_str()),
    m_CalibTableFilePath(calibTableFilePath),
    m_ImageIndex(0)
{
    m_ImageList = loadImageList(calibrationSetPath);
    std::random_shuffle(m_ImageList.begin(), m_ImageList.end(), [](int i) { return rand() % i; });
    NV_CUDA_CHECK(cudaMalloc(&m_DeviceInput, m_InputCount * sizeof(float)));
}

bool Int8EntropyCalibrator::getBatch(void* bindings[], const char* names[], int nbBindings)
{
    if (m_ImageIndex + m_BatchSize >= m_ImageList.size()) return false;
    float* trtInput = new float[m_InputSize * m_BatchSize];
    for (int j = m_ImageIndex; j < m_ImageIndex + m_BatchSize; ++j)
    {
        DsImage inputImage(m_ImageList[j], m_InputH, m_InputW);
        const float* data = inputImage.getImageData();
        for (int i = 0; i < m_InputSize; ++i)
        { trtInput[(j - m_ImageIndex) * m_InputSize + i] = clamp(data[i], 0.0, 1.0); } }
    m_ImageIndex += m_BatchSize;

    NV_CUDA_CHECK(
        cudaMemcpy(m_DeviceInput, trtInput, m_InputCount * sizeof(float), cudaMemcpyHostToDevice));
    assert(!strcmp(names[0], m_InputBlobName));
    bindings[0] = m_DeviceInput;
    delete trtInput;
    return true;
}

const void* Int8EntropyCalibrator::readCalibrationCache(size_t& length)
{
    m_CalibrationCache.clear();
    assert(!m_CalibTableFilePath.empty());
    std::ifstream input(m_CalibTableFilePath, std::ios::binary);
    input >> std::noskipws;
    if (m_ReadCache && input.good())
        std::copy(std::istream_iterator<char>(input), std::istream_iterator<char>(),
                  std::back_inserter(m_CalibrationCache));

    length = m_CalibrationCache.size();
    return length ? &m_CalibrationCache[0] : nullptr;
}

void Int8EntropyCalibrator::writeCalibrationCache(const void* cache, size_t length)
{
    assert(!m_CalibTableFilePath.empty());
    std::ofstream output(m_CalibTableFilePath, std::ios::binary);
    output.write(reinterpret_cast<const char*>(cache), length);
    output.close();
}