#include "cuda.h"
#include "cuda_util.h"
#include "utility_functions.h"
#include "weight_clipper.h"
#include "weight_clipper_cuda.h"
#include "gtest/gtest.h"
#include <chrono>
#include <memory>
#include <random>

#define TOLERANCE 1e-5

#ifdef RPU_USE_DOUBLE
typedef double num_t;
#else
typedef float num_t;
#endif

namespace {

using namespace RPU;

class WeightClipperTestFixture : public ::testing::TestWithParam<bool> {
public:
  void SetUp() {

    x_size = 100;
    d_size = 130;
    size = d_size * x_size;

    w = new num_t[size];
    v = new num_t[size]();

    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator{seed};
    std::normal_distribution<num_t> ndist{0.0, 1.0};
    auto nrnd = std::bind(ndist, generator);

    wclipper = make_unique<WeightClipper<num_t>>(x_size, d_size);
    wclipper_cuda = make_unique<WeightClipperCuda<num_t>>(&context, x_size, d_size);

    // just assign some numbers from the weigt matrix
    for (int i = 0; i < size; i++) {
      w[i] = nrnd();
    }

    dev_w = make_unique<CudaArray<num_t>>(&context, size, w);
    dev_w->assignTranspose(w, d_size, x_size);
    context.synchronize();
  }

  void TearDown() {
    delete[] w;
    delete[] v;
  }

  int x_size, d_size;
  int size;
  WeightClipParameter wclpar;
  std::unique_ptr<WeightClipper<num_t>> wclipper;
  std::unique_ptr<WeightClipperCuda<num_t>> wclipper_cuda;
  num_t *w, *v;
  std::unique_ptr<CudaArray<num_t>> dev_w;
  CudaContext context{0};
};

TEST_F(WeightClipperTestFixture, FixedValue) {

  wclpar.type = WeightClipType::FixedValue;
  wclpar.fixed_value = 0.4;

  wclipper->apply(w, wclpar);
  wclipper_cuda->apply(dev_w->getData(), wclpar);
  context.synchronize();

  num_t w_max = Find_Absolute_Max<num_t>(w, size);

  dev_w->copyTo(v);
  num_t dev_w_max = Find_Absolute_Max<num_t>(v, size);
  std::cout << "w_max " << w_max << ", dev_w_max " << dev_w_max << std::endl;
  EXPECT_FLOAT_EQ(w_max, dev_w_max);
  EXPECT_FLOAT_EQ(w_max, wclpar.fixed_value);
}

TEST_F(WeightClipperTestFixture, LayerGaussian) {

  wclpar.type = WeightClipType::LayerGaussian;
  wclpar.sigma = 0.4;

  wclipper->apply(w, wclpar);
  wclipper_cuda->apply(dev_w->getData(), wclpar);
  context.synchronize();

  num_t w_max = Find_Absolute_Max<num_t>(w, size);

  dev_w->copyTo(v);
  num_t dev_w_max = Find_Absolute_Max<num_t>(v, size);
  std::cout << "w_max " << w_max << ", dev_w_max " << dev_w_max << std::endl;
  EXPECT_NEAR(w_max, dev_w_max, 0.01);
  EXPECT_NEAR(w_max, wclpar.sigma, 0.01);
}

TEST_F(WeightClipperTestFixture, AverageChannelMax) {

  wclpar.type = WeightClipType::AverageChannelMax;

  wclipper->apply(w, wclpar);
  wclipper_cuda->apply(dev_w->getData(), wclpar);
  context.synchronize();

  num_t w_max = Find_Absolute_Max<num_t>(w, size);

  dev_w->copyTo(v);
  num_t dev_w_max = Find_Absolute_Max<num_t>(v, size);
  std::cout << "w_max " << w_max << ", dev_w_max " << dev_w_max << std::endl;
  EXPECT_NEAR(w_max, dev_w_max, 0.0001);
}

} // namespace

int main(int argc, char **argv) {
  resetCuda();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
