#pragma once

#include <ranges>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <optional>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include "mandelbrot_fractal_utils.hpp"
#include "mandelbrot_sender.hpp"
#include "types.hpp"

namespace rv = std::views;
namespace rs = std::ranges;
using namespace std::chrono;

// Фабрика сендера расчета фрактала Мандельброта.
class MandelbrotRenderer {
    exec::static_thread_pool thread_pool_;
    std::optional<ColorMatrix> color_buf_;  // Буфер пересчета pixels в color.

    public:
    explicit MandelbrotRenderer(std::uint32_t num_threads = std::thread::hardware_concurrency()):
        thread_pool_ {num_threads} {}

    // Асинхронная версия расчета фрактала на сендерах.
    template<size_t N>
    [[nodiscard]] auto RenderAsync(mandelbrot::ViewPort viewport, RenderSettings settings, bool need_rerender) {
        // Оптимизация произовдительности: аллокация буфера пересчета pixels в color.
        if(!color_buf_.has_value()) {
            color_buf_.emplace(settings.height,
                               std::vector<mandelbrot::RgbColor>(settings.width, mandelbrot::RgbColors::BLACK));
        }

        auto scheduler        = thread_pool_.get_scheduler();
        const auto start_time = steady_clock::now();

        // Деление экрана на полосы типа PixelRegion.
        auto split_into_stripes = [&](uint32_t n) {
            std::vector<PixelRegion> stripes;
            stripes.reserve(n);

            const uint32_t base_height = settings.height / n;
            const uint32_t remainder   = settings.height % n;

            uint32_t current_row = 0;
            for(auto stripe_id: rv::iota(0u, n)) {
                const uint32_t extra         = (stripe_id == n - 1) ?
                                                   remainder :
                                                   0;  // Учитываем остаток строк экрана, если число полос экрана не кратно N.
                const uint32_t stripe_height = base_height + extra;
                stripes.emplace_back(current_row, current_row + stripe_height, 0, settings.width);
                current_row += stripe_height;
            }

            return stripes;
        };

        auto all_stripes = split_into_stripes(N);

        // Сендер для одной PixelRegion полосы: описывает расчет фрактала для полосы.
        auto single_stripe_sender = [&](PixelRegion region) {
            return stdexec::on(scheduler, MakeMandelbrotSender(viewport, settings, region, need_rerender) |
                                              stdexec::then([this, region, settings](PixelMatrix&& pixels) {
                                                  WriteIterationsToColorBuffer(pixels, region, settings);
                                              }));
        };

        // Проблема: в compiletime нам известно число полос, на которые нужно разделить экран. stdexec::when_all
        // принимает variadic args, поэтому, чтобы в compiletime в when_all передать точное число single_stripe_sender
        // для запуска, необходимо воспользоваться fold_expr.

        // when_all по всем полосам. Асинхронно запускает расчет фракталов для N-полос. Результат работы: готовится
        // финальный сендер, в котором во все N-полос записан необходимый цвет.
        auto when_all_stripes_sender = [&]<size_t... Indices>(std::index_sequence<Indices...>) {
            return stdexec::when_all(single_stripe_sender(all_stripes[Indices])...);
        }(std::make_index_sequence<N> {});

        // Адаптер результата. Упаковка результата в RenderResult.
        auto to_render_adapter = stdexec::then([&]() -> RenderResult {
            RenderResult result {};
            result.viewport    = viewport;
            result.settings    = settings;
            result.color_data  = *color_buf_;   // Забираем данные из статичного буфера.
            result.render_time = duration_cast<milliseconds>(steady_clock::now() - start_time);
            return result;
        });

        return when_all_stripes_sender | to_render_adapter;
    }

    private:
    // Конвертация без аллокаций, запись прямо в color_buf_.
    void WriteIterationsToColorBuffer(const PixelMatrix& pixels, const PixelRegion& region,
                                      const RenderSettings& settings) {
        auto& cb_ref = *color_buf_;

        for(auto y: rv::iota(0u, pixels.size())) {
            const auto region_y = region.start_row + y;
            if(region_y >= cb_ref.size()) {
                break;
            }
            for(auto x: rv::iota(0u, settings.width)) {
                // Оптимизация произовдительности: запись color в статичный буфер.
                // Data-race-free запись. Каждая полоса пишется в буфер только в свой диапазон строк, без пересечений.
                cb_ref[region_y][x] = mandelbrot::IterationsToColor(pixels[y][x], settings.max_iterations);
            }
        }
    }
};
