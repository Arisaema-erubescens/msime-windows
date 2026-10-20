// ImageMask: turns the alpha channel of an image into rectangles / a GDI region for shaped windows.
#include "msimeui/ImageMask.h"

#include <algorithm>
#include <utility>
#include <wincodec.h>
#include <wrl/client.h>

namespace msimeui
{
namespace
{
using Run = std::pair<LONG, LONG>;

void RowRuns(const BYTE *row, UINT width, BYTE threshold, std::vector<Run> &runs)
{
    runs.clear();
    UINT x = 0;
    while (x < width)
    {
        while (x < width && row[x * 4 + 3] <= threshold)
        {
            ++x;
        }
        const UINT start = x;
        while (x < width && row[x * 4 + 3] > threshold)
        {
            ++x;
        }
        if (x > start)
        {
            runs.emplace_back(static_cast<LONG>(start), static_cast<LONG>(x));
        }
    }
}

void FlushRuns(const std::vector<Run> &runs, LONG top, LONG bottom, std::vector<RECT> &rects)
{
    for (const Run &run : runs)
    {
        rects.push_back({run.first, top, run.second, bottom});
    }
}
} // namespace

std::vector<RECT> AlphaMaskRects(const BYTE *pixels, UINT width, UINT height, UINT stride, BYTE threshold)
{
    std::vector<RECT> rects;
    if (!pixels || width == 0 || height == 0 || stride < width * 4)
    {
        return rects;
    }
    // Runs of the rows since `groupTop`, all identical; a row with different runs closes them.
    std::vector<Run> group;
    std::vector<Run> runs;
    LONG groupTop = 0;
    for (UINT y = 0; y < height; ++y)
    {
        RowRuns(pixels + static_cast<size_t>(y) * stride, width, threshold, runs);
        if (runs != group)
        {
            FlushRuns(group, groupTop, static_cast<LONG>(y), rects);
            group.swap(runs);
            groupTop = static_cast<LONG>(y);
        }
    }
    FlushRuns(group, groupTop, static_cast<LONG>(height), rects);
    return rects;
}

HRGN CreateAlphaMaskRegion(const std::wstring &filePath, UINT width, UINT height, BYTE threshold)
{
    using Microsoft::WRL::ComPtr;
    if (filePath.empty() || width == 0 || height == 0)
    {
        return nullptr;
    }
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICBitmapScaler> scaler;
    ComPtr<IWICFormatConverter> converter;
    // Cache on load so the file is not held open once the frame is decoded.
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(factory.GetAddressOf()))) ||
        FAILED(factory->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                                  decoder.GetAddressOf())) ||
        FAILED(decoder->GetFrame(0, frame.GetAddressOf())) ||
        FAILED(factory->CreateBitmapScaler(scaler.GetAddressOf())) ||
        FAILED(scaler->Initialize(frame.Get(), width, height, WICBitmapInterpolationModeFant)) ||
        FAILED(factory->CreateFormatConverter(converter.GetAddressOf())) ||
        FAILED(converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom)))
    {
        return nullptr;
    }
    const UINT stride = width * 4;
    std::vector<BYTE> pixels(static_cast<size_t>(stride) * height);
    if (FAILED(converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data())))
    {
        return nullptr;
    }
    const std::vector<RECT> rects = AlphaMaskRects(pixels.data(), width, height, stride, threshold);
    if (rects.empty())
    {
        return nullptr;
    }
    std::vector<BYTE> buffer(sizeof(RGNDATAHEADER) + rects.size() * sizeof(RECT));
    auto *data = reinterpret_cast<RGNDATA *>(buffer.data());
    data->rdh.dwSize = sizeof(RGNDATAHEADER);
    data->rdh.iType = RDH_RECTANGLES;
    data->rdh.nCount = static_cast<DWORD>(rects.size());
    data->rdh.nRgnSize = static_cast<DWORD>(rects.size() * sizeof(RECT));
    data->rdh.rcBound = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    std::copy(rects.begin(), rects.end(), reinterpret_cast<RECT *>(data->Buffer));
    return ExtCreateRegion(nullptr, static_cast<DWORD>(buffer.size()), data);
}
} // namespace msimeui
