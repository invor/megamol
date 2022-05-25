#include "FlowTimeLabelFilter.h"

#include "imageseries/graph/GraphData2D.h"

#include "vislib/graphics/BitmapImage.h"

#include <array>
#include <deque>
#include <vector>

namespace megamol::ImageSeries::filter {

FlowTimeLabelFilter::FlowTimeLabelFilter(Input input) : input(std::move(input)) {}

FlowTimeLabelFilter::ImagePtr FlowTimeLabelFilter::operator()() {
    using Image = AsyncImageData2D::BitmapImage;

    // Wait for image data to be ready
    auto image = input.timeMap ? input.timeMap->getImageData() : nullptr;

    // Empty -> return nothing
    if (!image) {
        return nullptr;
    }

    if (image->GetChannelCount() != 1 || image->GetChannelType() != Image::ChannelType::CHANNELTYPE_WORD) {
        return nullptr;
    }

    // Create output image
    auto result = std::make_shared<Image>(image->Width(), image->Height(), 1, Image::ChannelType::CHANNELTYPE_BYTE);

    using Index = std::uint32_t;
    using Timestamp = std::uint16_t;

    const auto* dataIn = image->PeekDataAs<Timestamp>();
    auto* dataOut = result->PeekDataAs<std::uint8_t>();
    Index width = result->Width();
    Index height = result->Height();
    Index size = width * height;

    Timestamp timeThreshold = input.timeThreshold;

    Timestamp currentTimestamp = 0;
    Timestamp minimumTimestamp = input.minimumTimestamp;
    Timestamp maximumTimestamp = 0;

    for (Index i = 0; i < size; ++i) {
        auto timestamp = dataIn[i];
        // Init background/mask
        dataOut[i] = timestamp < minimumTimestamp ? LabelMask : LabelBackground;
        if (maximumTimestamp < timestamp && timestamp < input.maximumTimestamp) {
            maximumTimestamp = timestamp;
        }
    }

    std::vector<Index> floodQueue;
    std::vector<Index> nextFront;

    auto mark = [&](Index index, Label label) {
        // Skip already-visited pixels
        if (dataOut[index] != LabelBackground) {
            return;
        }

        auto sample = dataIn[index];
        if (sample == currentTimestamp) {
            // Mark as filled
            dataOut[index] = label;
            floodQueue.push_back(index);
        } else if (sample > currentTimestamp && sample < static_cast<std::uint32_t>(currentTimestamp) + timeThreshold) {
            // Mark as pending
            dataOut[index] = LabelFlow;
            nextFront.push_back(index);
        }
    };

    auto floodFill = [&](Index index, Label label) {
        floodQueue.clear();
        floodQueue.push_back(index);
        for (std::size_t stackIndex = 0; stackIndex < floodQueue.size(); ++stackIndex) {
            index = floodQueue[stackIndex];

            // Pixel is not on the right boundary
            if (index % width < width - 1) {
                mark(index + 1, label);
            }
            // Pixel is not on the left boundary
            if (index % width > 0) {
                mark(index - 1, label);
            }
            // Pixel is not on the bottom boundary
            if (index < size - width) {
                mark(index + width, label);
            }
            // Pixel is not on the top boundary
            if (index >= width) {
                mark(index - width, label);
            }
        }
    };

    // Phase 1: find initial frame
    for (currentTimestamp = minimumTimestamp; currentTimestamp <= maximumTimestamp; ++currentTimestamp) {
        Label frontLabel = LabelFirst;
        for (Index i = 0; i < size; ++i) {
            if (dataIn[i] == currentTimestamp) {
                dataOut[i] = frontLabel;

                // Remember old size of nextFront
                std::size_t nextFrontPrevSize = nextFront.size();

                // Perform speculative flood fill. If the region is big enough, keep the changes. Otherwise, revert them
                floodFill(i, frontLabel);
                if (floodQueue.size() >= input.minBlobSize) {
                    // Region large enough: advance label counter and keep filled pixels
                    if (frontLabel != LabelLast) {
                        frontLabel++;
                    }
                } else {
                    // Region too small: mask out any involved pixels and revert front queue
                    dataOut[i] = LabelMask;
                    // Mask flooded pixels
                    for (auto index : floodQueue) {
                        dataOut[index] = LabelMask;
                    }
                    // Revert queued pixels
                    for (std::size_t frontIndex = nextFrontPrevSize; frontIndex < nextFront.size(); ++frontIndex) {
                        dataOut[nextFront[frontIndex]] = LabelBackground;
                    }
                    nextFront.resize(nextFrontPrevSize);
                }
            }
        }

        // Enter phase 2 if any suitably sized region was found
        if (frontLabel != LabelFirst) {
            break;
        }
    }

    // Phase 2: follow fluid flow
    for (; currentTimestamp <= maximumTimestamp; ++currentTimestamp) {
        // Split pending pixels into sections with unique labels
        std::vector<Index> frontStack;
        Label frontLabel = LabelFirst;
        auto splitFront = [&](Index index) {
            if (dataOut[index] != LabelFlow) {
                return;
            }
            frontStack.clear();
            frontStack.push_back(index);
            dataOut[index] = frontLabel;
            while (!frontStack.empty()) {
                index = frontStack.back();
                frontStack.pop_back();

                int cx = index % width;
                int cy = index / width;
                for (int y = std::max<int>(0, cy - 2); y < std::min<int>(cy + 3, height); ++y) {
                    for (int x = std::max<int>(0, cx - 2); x < std::min<int>(cx + 3, width); ++x) {
                        Index nextIndex = x + y * width;
                        if (dataOut[nextIndex] == LabelFlow) {
                            frontStack.push_back(nextIndex);
                            dataOut[nextIndex] = frontLabel;
                        }
                    }
                }
            }
            if (frontLabel != LabelLast) {
                frontLabel++;
            }
        };

        // Split different non-connected fronts
        for (auto pendingIndex : nextFront) {
            splitFront(pendingIndex);
        }

        std::swap(frontStack, nextFront);
        nextFront.clear();
        for (auto pendingIndex : frontStack) {
            if (dataIn[pendingIndex] == currentTimestamp) {
                // Same timestamp: perform flood fill from this pixel
                floodFill(pendingIndex, dataOut[pendingIndex]);
            } else {
                // Preserve front pixels with a steeper timestamp gradient, requeueing them for next time
                nextFront.push_back(pendingIndex);
                dataOut[pendingIndex] = LabelFlow;
            }
        }
    }

    return std::const_pointer_cast<const Image>(result);
}

std::size_t FlowTimeLabelFilter::getByteSize() const {
    return input.timeMap ? input.timeMap->getByteSize() / 2 : 0;
}

AsyncImageData2D::Hash FlowTimeLabelFilter::getHash() const {
    return util::computeHash(input.timeMap, input.blobCountLimit, input.minBlobSize, input.timeThreshold,
        input.minimumTimestamp, input.maximumTimestamp);
}

} // namespace megamol::ImageSeries::filter
