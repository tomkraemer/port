#include <softcamcore/FrameBuffer.h>
#include <gtest/gtest.h>

#include <vector>
#include <atomic>
#include <thread>


namespace {
namespace sc = softcam;


TEST(FrameBuffer, Basic1) {
    auto fb = sc::FrameBuffer::create(320, 240, 60);

    EXPECT_TRUE( fb );
    EXPECT_NE( fb.handle(), nullptr );
    EXPECT_EQ( fb.width(), 320 );
    EXPECT_EQ( fb.height(), 240 );
    EXPECT_EQ( fb.framerate(), 60.0f );
    EXPECT_EQ( fb.active(), true );
    EXPECT_EQ( fb.connected(), false );
}

TEST(FrameBuffer, Basic2) {
    auto sender = sc::FrameBuffer::create(320, 240, 60);
    auto receiver = sc::FrameBuffer::open();

    EXPECT_TRUE( sender );
    EXPECT_TRUE( receiver );
    EXPECT_NE( sender.handle(), nullptr );
    EXPECT_NE( receiver.handle(), nullptr );
    EXPECT_NE( sender.handle(), receiver.handle() );
    EXPECT_EQ( sender.width(), 320 );
    EXPECT_EQ( sender.height(), 240 );
    EXPECT_EQ( sender.framerate(), 60.0f );
    EXPECT_EQ( sender.active(), true );
    EXPECT_EQ( sender.connected(), true );
    EXPECT_EQ( receiver.width(), 320 );
    EXPECT_EQ( receiver.height(), 240 );
    EXPECT_EQ( receiver.framerate(), 60.0f );
    EXPECT_EQ( receiver.active(), true );
    EXPECT_EQ( receiver.connected(), true );
}

TEST(FrameBuffer, FramerateIsOptional) {
    auto fb = sc::FrameBuffer::create(320, 240);

    EXPECT_TRUE( fb );
    EXPECT_NE( fb.handle(), nullptr );
    EXPECT_EQ( fb.framerate(), 0.0f );
}

TEST(FrameBuffer, InvalidArgs) {
    {
        auto fb = sc::FrameBuffer::create(0, 240);
        EXPECT_FALSE( fb );
        EXPECT_EQ( fb.handle(), nullptr );
        EXPECT_EQ( fb.width(), 0 );
        EXPECT_EQ( fb.height(), 0 );
        EXPECT_EQ( fb.framerate(), 0.0f );
        EXPECT_EQ( fb.active(), false );
    }{
        auto fb = sc::FrameBuffer::create(320, 0);
        EXPECT_FALSE( fb );
        EXPECT_EQ( fb.handle(), nullptr );
    }{
        auto fb = sc::FrameBuffer::create(0, 0);
        EXPECT_FALSE( fb );
        EXPECT_EQ( fb.handle(), nullptr );
    }{
        auto fb = sc::FrameBuffer::create(-320, 240);
        EXPECT_FALSE( fb );
        EXPECT_EQ( fb.handle(), nullptr );
    }{
        auto fb = sc::FrameBuffer::create(320, -240);
        EXPECT_FALSE( fb );
        EXPECT_EQ( fb.handle(), nullptr );
    }{
        auto fb = sc::FrameBuffer::create(320, 240, -60);
        EXPECT_FALSE( fb );
        EXPECT_EQ( fb.handle(), nullptr );
    }
}

TEST(FrameBuffer, TooLarge) {
    {
        auto fb = sc::FrameBuffer::create(32000, 240);
        EXPECT_FALSE( fb );
        EXPECT_EQ( fb.handle(), nullptr );
    }{
        auto fb = sc::FrameBuffer::create(320, 24000);
        EXPECT_FALSE( fb );
        EXPECT_EQ( fb.handle(), nullptr );
    }
}

TEST(FrameBuffer, OpenBeforeCreateFails) {
    auto receiver = sc::FrameBuffer::open();
    auto sender = sc::FrameBuffer::create(320, 240);

    EXPECT_FALSE( receiver );
    EXPECT_TRUE( sender );
    EXPECT_EQ( receiver.handle(), nullptr );
    EXPECT_NE( sender.handle(), nullptr );
    EXPECT_EQ( sender.connected(), false );
}

TEST(FrameBuffer, MultipleCreateFails) {
    auto fb1 = sc::FrameBuffer::create(320, 240, 60);
    auto fb2 = sc::FrameBuffer::create(320, 240, 60);

    EXPECT_TRUE( fb1 );
    EXPECT_FALSE( fb2 );
    EXPECT_NE( fb1.handle(), nullptr );
    EXPECT_EQ( fb2.handle(), nullptr );
}

TEST(FrameBuffer, MultipleOpenSucceeds) {
    auto sender = sc::FrameBuffer::create(320, 240, 60);
    auto receiver1 = sc::FrameBuffer::open();
    auto receiver2 = sc::FrameBuffer::open();

    EXPECT_TRUE( sender );
    EXPECT_TRUE( receiver1 );
    EXPECT_TRUE( receiver2 );
    EXPECT_NE( sender.handle(), nullptr );
    EXPECT_NE( receiver1.handle(), nullptr );
    EXPECT_NE( receiver2.handle(), nullptr );
    EXPECT_NE( sender.handle(), receiver1.handle() );
    EXPECT_NE( sender.handle(), receiver2.handle() );
    EXPECT_NE( receiver1.handle(), receiver2.handle() );
    EXPECT_EQ( sender.connected(), true );
}

TEST(FrameBuffer, WriteIncreasesFrameCounter) {
    auto fb = sc::FrameBuffer::create(320, 240, 60);
    EXPECT_EQ( fb.frameCounter(), 0 );

    std::vector<uint8_t> image(320 * 240 * 3, 255);
    fb.write(image.data());
    EXPECT_EQ( fb.frameCounter(), 1 );

    fb.write(image.data());
    EXPECT_EQ( fb.frameCounter(), 2 );
}

TEST(FrameBuffer, DeactivateTurnsActiveFlagOff) {
    auto sender = sc::FrameBuffer::create(320, 240, 60);
    auto receiver = sc::FrameBuffer::open();
    sender.deactivate();

    EXPECT_TRUE( sender );
    EXPECT_NE( sender.handle(), nullptr );
    EXPECT_EQ( sender.width(), 320 );
    EXPECT_EQ( sender.height(), 240 );
    EXPECT_EQ( sender.framerate(), 60.0f );
    EXPECT_EQ( sender.frameCounter(), 0 );
    EXPECT_EQ( sender.active(), false );
    EXPECT_TRUE( receiver );
    EXPECT_EQ( receiver.active(), false );
}

TEST(FrameBuffer, WaitForNewFrameTimesOut) {
    const float TIMEOUT_TIME = 0.3f;
    auto fb = sc::FrameBuffer::create(320, 240, 60);

    std::atomic<int> pos = 0;
    std::thread th([&]{
        bool ret = fb.waitForNewFrame(0, TIMEOUT_TIME);
        EXPECT_EQ( ret, true );
        pos = 1;
    });

    sc::Timer::sleep(0.01f);
    EXPECT_EQ( pos, 0 );
    sc::Timer::sleep(TIMEOUT_TIME + 0.1f);
    EXPECT_EQ( pos, 1 );
    th.join();
}

TEST(FrameBuffer, WaitForNewFrameStopsAfterNewFrameArrived) {
    const float TIMEOUT_TIME = 2.0f;
    auto fb = sc::FrameBuffer::create(320, 240, 60);

    auto frame_count = fb.frameCounter();
    std::atomic<int> pos = 0;
    std::thread th([&]{
        bool ret = fb.waitForNewFrame(frame_count, TIMEOUT_TIME);
        EXPECT_EQ( ret, true );
        pos = 1;
    });

    sc::Timer::sleep(0.1f);
    EXPECT_EQ( pos, 0 );

    std::vector<uint8_t> image(320 * 240 * 3, 255);
    fb.write(image.data());
    sc::Timer::sleep(0.1f);
    EXPECT_EQ( pos, 1 );
    th.join();
}

TEST(FrameBuffer, WaitForNewFrameStopsIfDeactivated) {
    const float TIMEOUT_TIME = 2.0f;
    auto fb = sc::FrameBuffer::create(320, 240, 60);

    auto frame_count = fb.frameCounter();
    std::atomic<int> pos = 0;
    std::thread th([&]{
        bool ret = fb.waitForNewFrame(frame_count, TIMEOUT_TIME);
        EXPECT_EQ( ret, false );
        pos = 1;
    });

    sc::Timer::sleep(0.1f);
    EXPECT_EQ( pos, 0 );

    std::vector<uint8_t> image(320 * 240 * 3, 255);
    fb.deactivate();
    sc::Timer::sleep(0.1f);
    EXPECT_EQ( pos, 1 );
    th.join();
}

TEST(FrameBuffer, WaitForNewFrameStopsWhenWatchdogTimeouts) {
    const float WATCHDOG_TIMEOUT = 1.0f;
    const float TEST_TIMEOUT = WATCHDOG_TIMEOUT + 1.0f;
    auto fb = sc::FrameBuffer::create(320, 240, 60);

    std::atomic<int> pos = 0;
    std::thread th([&]{
        auto receiver = sc::FrameBuffer::open();
        auto frame_count = receiver.frameCounter();
        bool ret = receiver.waitForNewFrame(frame_count, TEST_TIMEOUT);
        EXPECT_EQ( ret, false );
        pos = 1;
    });

    sc::Timer::sleep(0.1f);
    EXPECT_EQ( pos, 0 );

    fb.release();
    sc::Timer::sleep(0.1f);
    EXPECT_EQ( pos, 0 );

    sc::Timer::sleep(WATCHDOG_TIMEOUT + 0.1f);
    EXPECT_EQ( pos, 1 );
    th.join();
}

TEST(FrameBuffer, ReleaseInvalidateItself) {
    auto fb = sc::FrameBuffer::create(320, 240, 60);
    fb.release();

    EXPECT_FALSE( fb );
    EXPECT_EQ( fb.handle(), nullptr );
    EXPECT_EQ( fb.width(), 0 );
    EXPECT_EQ( fb.height(), 0 );
    EXPECT_EQ( fb.framerate(), 0.0f );
    EXPECT_EQ( fb.frameCounter(), 0 );
    EXPECT_EQ( fb.active(), false );
    EXPECT_EQ( fb.connected(), false );
}

TEST(FrameBuffer, ReleaseOnReceiverDisconnects) {
    auto sender = sc::FrameBuffer::create(320, 240, 60);
    auto receiver = sc::FrameBuffer::open();
    receiver.release();

    EXPECT_FALSE( receiver );
    EXPECT_EQ( receiver.handle(), nullptr );
    EXPECT_EQ( receiver.width(), 0 );
    EXPECT_EQ( receiver.height(), 0 );
    EXPECT_EQ( receiver.framerate(), 0.0f );
    EXPECT_EQ( receiver.frameCounter(), 0 );
    EXPECT_EQ( receiver.active(), false );
    EXPECT_EQ( receiver.connected(), false );

    EXPECT_TRUE( sender );
    EXPECT_NE( sender.handle(), nullptr );
    EXPECT_EQ( sender.width(), 320 );
    EXPECT_EQ( sender.height(), 240 );
    EXPECT_EQ( sender.framerate(), 60.0f );
    EXPECT_EQ( sender.active(), true );
}

} //namespace