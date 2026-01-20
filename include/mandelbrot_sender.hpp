#pragma once

#include <algorithm>
#include <cstdint>
#include <exception>
#include <utility>
#include <vector>
#include <ranges>

#include <stdexec/execution.hpp>

#include "types.hpp"

namespace rv = std::views;
namespace rs = std::ranges;

// MandelbrotSender вычисляет количество итераций для области пикселей PixelRegion.
    // Возвращает PixelMatrix - итерации для каждого пикселя. Далее в Renderer эти итерации преобразуются в цвета.
    template<typename Receiver>
    struct MandelbrotOperationState {
        using operation_state_concept = STDEXEC::operation_state_t;

        Receiver receiver_;
        mandelbrot::ViewPort viewport_;
        RenderSettings settings_;
        PixelRegion region_;
        bool need_rerender_;

        template<typename R>
        explicit MandelbrotOperationState(R&& receiver, mandelbrot::ViewPort viewport, RenderSettings settings,
                                          PixelRegion region, bool need_rerender):
            receiver_(std::forward<R>(receiver)),
            viewport_(viewport),
            settings_(settings),
            region_(region),
            need_rerender_(need_rerender) {}

        void start() noexcept {
            try {
                run();
            }
            catch(...) {
                stdexec::set_error(std::move(receiver_), std::current_exception());
            }
        }

        private:
        void run() {
            // Если downstream запросил stop — завершаемся stopped для корректного выхода по закрытию окна.
            auto stoken = stdexec::get_stop_token(stdexec::get_env(receiver_));
            if(stoken.stop_requested()) {
                stdexec::set_stopped(std::move(receiver_));
                return;
            }

            // Если отрисовка не нужна — ничего не считаем.
            if(!need_rerender_) {
                stdexec::set_value(std::move(receiver_), PixelMatrix {});
                return;
            }

            // Вычисление итераций для региона.
            auto complex_pixels = ComputeIterationsForRegion(settings_, region_);
            stdexec::set_value(std::move(receiver_), std::move(complex_pixels));
        }

        // Расчет итераций для области экрана.
        PixelMatrix ComputeIterationsForRegion(const RenderSettings& settings, const PixelRegion& region) {
            const uint32_t width  = settings_.width;
            const uint32_t height = settings_.height;

            const uint32_t start_row = region.start_row;
            const uint32_t end_row   = std::min(region.end_row, height);

            PixelMatrix complex_pixels(end_row - start_row, std::vector<uint32_t>(width));

            for(auto local_y: rv::iota(0u, complex_pixels.size())) {
                const uint32_t y = start_row + local_y;

                for(auto x: rv::iota(0u, width)) {
                    const auto complex_pixel = mandelbrot::Pixel2DToComplex(x, y, viewport_, width, height);

                    complex_pixels[local_y][x] = mandelbrot::CalculateIterationsForPoint(
                        complex_pixel, settings.max_iterations, settings.escape_radius);
                }
            }

            return complex_pixels;
        }
    };

// Сам sender хранит параметры вычисления.
template <typename Receiver>
struct MandelbrotSender {
    using sender_concept = stdexec::sender_t;

    mandelbrot::ViewPort viewport_;
    RenderSettings settings_;
    PixelRegion region_;
    bool need_rerender_;

    template<typename Env>
    static auto get_completion_signatures(const Env&) {
        return stdexec::completion_signatures<stdexec::set_value_t(PixelMatrix),
                                              stdexec::set_error_t(std::exception_ptr), stdexec::set_stopped_t()> {};
    }

    template<typename R>
    auto connect(R&& r)  {
        return MandelbrotOperationState<std::decay_t<R>> {std::forward<R>(r), viewport_, settings_, region_,
                                                          need_rerender_};
    }
};

// Сендер для вычисление итераций для области экрана.
[[nodiscard]] inline auto MakeMandelbrotSender(mandelbrot::ViewPort viewport, RenderSettings settings,
                                               PixelRegion region, bool need_rerender) {
    return MandelbrotSender<void> {std::move(viewport), std::move(settings), std::move(region), need_rerender};
}
