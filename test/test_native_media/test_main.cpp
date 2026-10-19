#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "image_fit.h"
#include "liveview_focus_policy.h"
#include "liveview_reso_policy.h"
#include "mjpeg_stream.h"

namespace {

struct CapturedFrames {
  uint32_t callbacks = 0;
  std::vector<uint8_t> lastFrame;
};

void captureFrame(const uint8_t* data, size_t len, void* user) {
  auto* captured = static_cast<CapturedFrames*>(user);
  ++captured->callbacks;
  captured->lastFrame.assign(data, data + len);
}

void testContainFitPreservesFourByThreeFrame() {
  const ImageFitRect fit = calculateContainRect(320, 240, 240, 135);
  TEST_ASSERT_EQUAL_INT(180, fit.width);
  TEST_ASSERT_EQUAL_INT(135, fit.height);
  TEST_ASSERT_EQUAL_INT(30, fit.x);
  TEST_ASSERT_EQUAL_INT(0, fit.y);
}

void testContainFitPreservesWideFrame() {
  const ImageFitRect fit = calculateContainRect(640, 360, 240, 135);
  TEST_ASSERT_EQUAL_INT(240, fit.width);
  TEST_ASSERT_EQUAL_INT(135, fit.height);
  TEST_ASSERT_EQUAL_INT(0, fit.x);
  TEST_ASSERT_EQUAL_INT(0, fit.y);
}

void testContainFitLetterboxesTallFrame() {
  const ImageFitRect fit = calculateContainRect(135, 240, 240, 135);
  TEST_ASSERT_EQUAL_INT(76, fit.width);
  TEST_ASSERT_EQUAL_INT(135, fit.height);
  TEST_ASSERT_EQUAL_INT(82, fit.x);
  TEST_ASSERT_EQUAL_INT(0, fit.y);
}

void testContainFitRejectsInvalidDimensions() {
  const ImageFitRect fit = calculateContainRect(0, 240, 240, 135);
  TEST_ASSERT_EQUAL_INT(0, fit.width);
  TEST_ASSERT_EQUAL_INT(0, fit.height);
}

void testBeginRejectsInvalidInputs() {
  MjpegStream stream;
  uint8_t buffer[32] = {};
  CapturedFrames captured;

  TEST_ASSERT_FALSE(stream.begin(nullptr, sizeof(buffer), captureFrame, &captured));
  TEST_ASSERT_FALSE(stream.begin(buffer, 3, captureFrame, &captured));
  TEST_ASSERT_FALSE(stream.begin(buffer, sizeof(buffer), nullptr, &captured));
  TEST_ASSERT_EQUAL_size_t(0, stream.process(buffer, sizeof(buffer)));
}

void testDeliversFrameSplitAcrossChunks() {
  MjpegStream stream;
  uint8_t buffer[64] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t prefix[] = {0x00, 0x11, 0xFF};
  const uint8_t middle[] = {
      0xD8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0xFF,
  };
  const uint8_t suffix[] = {0xD9, 0x22};
  const uint8_t expected[] = {
      0xFF, 0xD8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0xFF, 0xD9,
  };

  TEST_ASSERT_EQUAL_size_t(0, stream.process(prefix, sizeof(prefix)));
  TEST_ASSERT_EQUAL_size_t(0, stream.process(middle, sizeof(middle)));
  TEST_ASSERT_EQUAL_size_t(1, stream.process(suffix, sizeof(suffix)));

  TEST_ASSERT_EQUAL_UINT32(1, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(1, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(0, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(sizeof(expected), captured.lastFrame.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, captured.lastFrame.data(), sizeof(expected));
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testDropsShortFrame() {
  MjpegStream stream;
  uint8_t buffer[64] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t shortFrame[] = {0xFF, 0xD8, 0x01, 0xFF, 0xD9};

  TEST_ASSERT_EQUAL_size_t(0, stream.process(shortFrame, sizeof(shortFrame)));
  TEST_ASSERT_EQUAL_UINT32(0, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(0, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(1, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testDropsOverflowFrameWhenEoiArrives() {
  MjpegStream stream;
  uint8_t buffer[8] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t frame[] = {
      0xFF, 0xD8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0xFF, 0xD9,
  };

  TEST_ASSERT_EQUAL_size_t(0, stream.process(frame, sizeof(frame)));
  TEST_ASSERT_EQUAL_UINT32(0, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(0, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(1, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testResetClearsPartialFrameOnly() {
  MjpegStream stream;
  uint8_t buffer[64] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t shortFrame[] = {0xFF, 0xD8, 0x01, 0xFF, 0xD9};
  TEST_ASSERT_EQUAL_size_t(0, stream.process(shortFrame, sizeof(shortFrame)));

  const uint8_t partial[] = {0xFF, 0xD8, 0x01, 0x02};
  TEST_ASSERT_EQUAL_size_t(0, stream.process(partial, sizeof(partial)));
  TEST_ASSERT_GREATER_THAN_size_t(0, stream.currentLength());

  stream.reset();

  const uint8_t suffix[] = {0x03, 0xFF, 0xD9};
  TEST_ASSERT_EQUAL_size_t(0, stream.process(suffix, sizeof(suffix)));
  TEST_ASSERT_EQUAL_UINT32(0, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(0, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(1, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testLiveViewResoListParsesStringAndBareTokens() {
  LiveViewResolution list[LIVEVIEW_RESO_LIST_CAPACITY];
  const char* props =
      "{\"model\":\"GR IIIx\",\"resoList\":[\"720x480\", \"360x240\",\"1280x720\"],\"liveState\":\"idle\"}";
  TEST_ASSERT_EQUAL_UINT32(3, parseLiveViewResoList(props, list, LIVEVIEW_RESO_LIST_CAPACITY));
  TEST_ASSERT_EQUAL_UINT16(720, list[0].width);
  TEST_ASSERT_EQUAL_UINT16(480, list[0].height);
  TEST_ASSERT_EQUAL_UINT16(360, list[1].width);
  TEST_ASSERT_EQUAL_UINT16(1280, list[2].width);
  TEST_ASSERT_EQUAL_UINT16(720, list[2].height);

  // Digits outside the array and malformed tokens are ignored.
  TEST_ASSERT_EQUAL_UINT32(1, parseLiveViewResoList("{\"battery\":80,\"resoList\":[\"640\",\"x9\",640x427]}",
                                                    list,
                                                    LIVEVIEW_RESO_LIST_CAPACITY));
  TEST_ASSERT_EQUAL_UINT16(427, list[0].height);
  TEST_ASSERT_EQUAL_UINT32(0, parseLiveViewResoList("{\"resoList\":[]}", list, LIVEVIEW_RESO_LIST_CAPACITY));
  TEST_ASSERT_EQUAL_UINT32(0, parseLiveViewResoList("{\"model\":\"GR\"}", list, LIVEVIEW_RESO_LIST_CAPACITY));
  TEST_ASSERT_EQUAL_UINT32(0, parseLiveViewResoList(nullptr, list, LIVEVIEW_RESO_LIST_CAPACITY));
  // Capacity bounds the output.
  TEST_ASSERT_EQUAL_UINT32(1, parseLiveViewResoList(props, list, 1));
}

void testLiveViewResolutionPicksSmallestMatchingAspect() {
  const LiveViewResolution offered[] = {{720, 480}, {360, 240}, {1280, 720}, {216, 120}, {432, 288}};
  const LiveViewResolution chosen = chooseLiveViewResolution(offered, 5, 216, 144, 720, 480);
  TEST_ASSERT_EQUAL_UINT16(360, chosen.width);
  TEST_ASSERT_EQUAL_UINT16(240, chosen.height);

  // Near-3:2 camera sizes still qualify.
  const LiveViewResolution nearAspect[] = {{720, 480}, {640, 427}};
  TEST_ASSERT_EQUAL_UINT16(640, chooseLiveViewResolution(nearAspect, 2, 216, 144, 720, 480).width);

  // GR IIIx V1.50 offers only the default and a larger size: never request
  // either by name, since reso=720x480 preceded a camera lock-up.
  const LiveViewResolution gr3xList[] = {{720, 480}, {1080, 720}};
  TEST_ASSERT_FALSE(chooseLiveViewResolution(gr3xList, 2, 216, 144, 720, 480).valid());
  // Nothing at least as large as the decode target, or at its aspect ratio.
  const LiveViewResolution tooSmall[] = {{180, 120}, {720, 405}};
  TEST_ASSERT_FALSE(chooseLiveViewResolution(tooSmall, 2, 216, 144, 720, 480).valid());
  TEST_ASSERT_FALSE(chooseLiveViewResolution(nullptr, 0, 216, 144, 720, 480).valid());
}

void testLiveViewFocusSettingFormatsRicohBody() {
  TEST_ASSERT_EQUAL_STRING("spot", LIVEVIEW_SPOT_FOCUS_SETTING);
  TEST_ASSERT_TRUE(liveviewFocusSettingAllowed("spot"));
  TEST_ASSERT_TRUE(liveviewFocusSettingAllowed("multiauto_center"));
  TEST_ASSERT_TRUE(liveviewFocusSettingIsCenterPoint("spot"));
  TEST_ASSERT_TRUE(liveviewFocusSettingIsCenterPoint("pinpoint"));
  TEST_ASSERT_FALSE(liveviewFocusSettingIsCenterPoint("snap"));
  TEST_ASSERT_FALSE(liveviewFocusSettingAllowed(""));
  TEST_ASSERT_FALSE(liveviewFocusSettingAllowed("not-a-mode"));

  char body[48];
  TEST_ASSERT_TRUE(formatLiveviewFocusSettingBody(body, sizeof(body), "spot"));
  TEST_ASSERT_EQUAL_STRING("focusSetting=spot", body);
  TEST_ASSERT_FALSE(formatLiveviewFocusSettingBody(body, sizeof(body), "not-a-mode"));
  TEST_ASSERT_FALSE(formatLiveviewFocusSettingBody(nullptr, 48, "spot"));

  TEST_ASSERT_EQUAL_STRING("af=on&pos=50,50", LIVEVIEW_SHOOT_AF_ON_BODY);
  TEST_ASSERT_TRUE(formatLiveviewShootAfOnBody(body, sizeof(body)));
  TEST_ASSERT_EQUAL_STRING("af=on&pos=50,50", body);
  TEST_ASSERT_FALSE(formatLiveviewShootAfOnBody(nullptr, 24));
  TEST_ASSERT_FALSE(formatLiveviewShootAfOnBody(body, 8));
}

void testRicohHttpJsonTreatsErrMsgAsTheRealStatus() {
  TEST_ASSERT_TRUE(ricohHttpJsonSucceeded(""));
  TEST_ASSERT_TRUE(ricohHttpJsonSucceeded("{\"errCode\":200,\"errMsg\":\"OK\"}"));
  TEST_ASSERT_TRUE(ricohHttpJsonSucceeded("{\"captureId\":\"1\"}"));
  TEST_ASSERT_FALSE(ricohHttpJsonSucceeded("{\"errCode\":412,\"errMsg\":\"Precondition Failed\"}"));
  TEST_ASSERT_FALSE(ricohHttpJsonSucceeded("{ \"errMsg\" : \"Camera Busy\" }"));
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(testContainFitPreservesFourByThreeFrame);
  RUN_TEST(testContainFitPreservesWideFrame);
  RUN_TEST(testContainFitLetterboxesTallFrame);
  RUN_TEST(testContainFitRejectsInvalidDimensions);
  RUN_TEST(testLiveViewResoListParsesStringAndBareTokens);
  RUN_TEST(testLiveViewResolutionPicksSmallestMatchingAspect);
  RUN_TEST(testLiveViewFocusSettingFormatsRicohBody);
  RUN_TEST(testRicohHttpJsonTreatsErrMsgAsTheRealStatus);
  RUN_TEST(testBeginRejectsInvalidInputs);
  RUN_TEST(testDeliversFrameSplitAcrossChunks);
  RUN_TEST(testDropsShortFrame);
  RUN_TEST(testDropsOverflowFrameWhenEoiArrives);
  RUN_TEST(testResetClearsPartialFrameOnly);
  return UNITY_END();
}
