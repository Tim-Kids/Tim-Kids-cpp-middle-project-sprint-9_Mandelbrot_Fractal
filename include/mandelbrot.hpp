#pragma once

#include "mandelbrot_renderer.hpp"

class CalculateMandelbrotAsyncSender {  // Адаптер для запуска сендера MandelbrotSender при запуске main pipeline.
    public:
    explicit CalculateMandelbrotAsyncSender(AppState& state, RenderSettings render_settings,
                                            MandelbrotRenderer& renderer):
        state_(state),
        render_settings_(render_settings),
        renderer_(renderer) {}

    using sender_concept = stdexec::sender_t;

    template<typename Env>
    static auto get_completion_signatures(const Env&) {
        return stdexec::completion_signatures<stdexec::set_value_t(RenderResult),
                                              stdexec::set_error_t(std::exception_ptr),
            stdexec::set_stopped_t()> /*Фактически здесь никогда не используется, потому что класс ялвяется адаптером по
                                         отправке sender в другой sender*/
            {};
    }

    template<typename Receiver>
    auto connect(Receiver&& receiver) {
        // Получаем MandelbrotSender, который описывает расчет фрактала Мандельброта.
        auto mandelbrot_sender =
            renderer_.RenderAsync<THREAD_POOL_SIZE>(state_.viewport, render_settings_, state_.need_rerender);

        // Возваращем состояние операции CalculateMandelbrotAsyncSender на основе сендера MandelbrotSender. Модель
        // stdexec сама его запустит при запуске main pipeline, а его lifetime = runtime.
        return stdexec::connect(std::move(mandelbrot_sender), std::forward<Receiver>(receiver));
    }

    private:
    AppState& state_;
    RenderSettings render_settings_;
    MandelbrotRenderer& renderer_;
};
