#ifndef SRC_IMAGESERIES_FILTER_CONVOLUTION2DFILTER_HPP_
#define SRC_IMAGESERIES_FILTER_CONVOLUTION2DFILTER_HPP_

#include "imageseries/AsyncImageData2D.h"

#include <memory>

namespace megamol::ImageSeries::filter {

class Convolution2DFilter {
public:
    using AsyncImagePtr = std::shared_ptr<const AsyncImageData2D>;
    using ImagePtr = std::shared_ptr<const AsyncImageData2D::BitmapImage>;

    struct Input {
        AsyncImagePtr image;
        std::vector<float> kernelX = {1};
        std::vector<float> kernelY = {1};
    };

    Convolution2DFilter(Input input);
    ImagePtr operator()();

    std::size_t getByteSize() const;

    static std::vector<float> makeGaussianKernel(float sigma, std::size_t radius);

private:
    Input input;
};

} // namespace megamol::ImageSeries::filter


#endif