#include <gtest/gtest.h>
#include <memory>

#include <stdexec/execution.hpp>

#include "mandelbrot_fractal_utils.hpp"
#include "mandelbrot_renderer.hpp"
#include "mandelbrot_sender.hpp"
#include "mandelbrot.hpp"
#include "types.hpp"

// Ресивер-хелпер.
struct SimpleReceiver {
    using receiver_concept = stdexec::receiver_t;

    struct State {
        bool value_called   = false;
        bool error_called   = false;
        bool stopped_called = false;
    };

    std::shared_ptr<State> state = std::make_shared<State>();

    template<typename... T>
    void set_value(T&&...) noexcept {
        state->value_called = true;
    }

    template<typename Err>
    void set_error(Err&&) noexcept {
        state->error_called = true;
    }

    void set_stopped() noexcept {
        state->stopped_called = true;
    }

    auto get_env() const noexcept {
        return stdexec::empty_env{};
    }
};


// Возвращаем непустое значение в MakeMandelbrotSender, если флаг расчета пикселей true.
TEST(MandelbrotSender, ProducesValueWhenRerenderNeeded) {
    RenderSettings settings {};
    mandelbrot::ViewPort viewport {};
    PixelRegion region {0, 10, 0, settings.width};

    auto sender = MakeMandelbrotSender(viewport, settings, region, true);
    auto result = stdexec::sync_wait(std::move(sender));

    ASSERT_TRUE(result.has_value());
    const auto& matrix = std::get<0>(result.value());
    ASSERT_FALSE(matrix.empty());
}

// Ничего не считаем в MakeMandelbrotSender, если флаг расчета пикселей false.
TEST(MandelbrotSender, ProducesEmptyMatrixWhenNoRerender) {
    RenderSettings settings;
    mandelbrot::ViewPort vp;
    PixelRegion region {0, 10, 0, settings.width};

    auto sender = MakeMandelbrotSender(vp, settings, region, false);
    auto result = stdexec::sync_wait(std::move(sender));

    ASSERT_TRUE(result.has_value());
    const auto& matrix = std::get<0>(result.value());
    ASSERT_TRUE(matrix.empty());
}

// Если в pipeline один из сендеров в upstream завершился с set_stopped, то MandelbrotSender сендер должен его прокинуть
// дальше в downstream.
TEST(MandelbrotSender, RespectsStopToken) {
    RenderSettings settings {};
    mandelbrot::ViewPort viewport {};
    PixelRegion region {0, 10, 0, settings.width};

    auto pipeline = stdexec::just_stopped() |
                    stdexec::then([&] { return MakeMandelbrotSender(viewport, settings, region, true); }) |
                    stdexec::let_stopped([&] { return stdexec::just(); });

    auto result = stdexec::sync_wait(std::move(pipeline));
    ASSERT_TRUE(result.has_value());
}

// Проверка взаимодействия интерфейса pipeline проекта с кастомным ресивером: set_value.
TEST(Pipeline, CustomReceiverIsInvoked) {
    RenderSettings settings{};
    mandelbrot::ViewPort viewport{};
    PixelRegion region{0, 10, 0, settings.width};

    SimpleReceiver receiver;
    // shared_ptr поможет при копировани receiver в operation state сохранить ссылки на оригинальные State-флаги.
    auto state = receiver.state;

    auto sender = MakeMandelbrotSender(viewport, settings, region, true);
    auto op = stdexec::connect(std::move(sender), receiver);

    stdexec::start(op);

    ASSERT_TRUE(state->value_called);   // state по-прежнему живой.
    ASSERT_FALSE(state->error_called);   // state по-прежнему живой.
    ASSERT_FALSE(state->stopped_called);   // state по-прежнему живой.
}

// Проверка взаимодействия интерфейса pipeline проекта с кастомным ресивером: set_stopped.
TEST(ProjectPipeline, CustomReceiverGetsStoppedFromMandelbrotSender) {
    RenderSettings settings {};
    mandelbrot::ViewPort viewport {};
    PixelRegion region {0, 10, 0, settings.width};

    SimpleReceiver receiver;
    auto st = receiver.state;

    auto pipeline = stdexec::just_stopped() |
                    stdexec::then([&] { return MakeMandelbrotSender(viewport, settings, region, true); }) |
                    stdexec::let_stopped([&] { return stdexec::just_stopped(); });

    auto op = stdexec::connect(std::move(pipeline), receiver);
    stdexec::start(op);

    ASSERT_TRUE(st->stopped_called);  // MandelbrotSender отловит just_stopped, выставит set_stopped и прокинет в
                                      // downstream, а let_stopped запишет в ресивер финальный set_stopped.
    ASSERT_FALSE(st->error_called);
    ASSERT_FALSE(st->value_called);
}


// Проверка взаимодействия интерфейса pipeline проекта с кастомным ресивером: set_error.
TEST(ProjectPipeline, CustomReceiverGetsErrorFromProjectSenderChain) {
    RenderSettings settings{};
    mandelbrot::ViewPort viewport{};
    PixelRegion region{0, 10, 0, settings.width};

    SimpleReceiver receiver;
    auto st = receiver.state;

    auto pipeline =
        MakeMandelbrotSender(viewport, settings, region, true)
        | stdexec::then([](PixelMatrix&&) -> int {
              throw std::runtime_error("Error occurred!");
          });

    auto op = stdexec::connect(std::move(pipeline), receiver);
    stdexec::start(op);

    ASSERT_TRUE(st->error_called);
    ASSERT_FALSE(st->value_called);
    ASSERT_FALSE(st->stopped_called);
}


// RenderAsync корректно рассчитывает пиксели и цвета для отрисовки фрактала на N потоках.
TEST(MandelbrotRenderer, RenderAsyncProducesFrame) {
    const auto N = 4;
    MandelbrotRenderer renderer {N};
    RenderSettings settings {.width = 64, .height = 64};
    mandelbrot::ViewPort viewport;

    auto sender = renderer.RenderAsync<N>(viewport, settings, true);
    auto result = stdexec::sync_wait(std::move(sender));

    ASSERT_TRUE(result.has_value());

    const auto val = std::get<0>(result.value());

    ASSERT_EQ(val.color_data.size(), settings.height);
    ASSERT_EQ(val.color_data[0].size(), settings.width);
}

// Адаптер для запуска MandelbrotSender работает корректно.
TEST(Pipeline, MandelbrotWithoutRendering) {
    MandelbrotRenderer renderer {4};
    RenderSettings settings {};
    AppState state {};
    state.need_rerender = true;

    auto sender = CalculateMandelbrotAsyncSender {state, settings, renderer};
    auto result = stdexec::sync_wait(std::move(sender));

    ASSERT_TRUE(result.has_value());
}
