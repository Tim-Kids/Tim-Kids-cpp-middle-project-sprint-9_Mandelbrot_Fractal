#pragma once

#include <SFML/Graphics.hpp>
#include <stdexec/execution.hpp>

#include "types.hpp"

#include <sys/stat.h>

class SfmlEventHandler {
    public:
    SfmlEventHandler(sf::RenderWindow& window, RenderSettings render_settings, AppState& state, sf::Clock& zoom_clock):
        window_ {window},
        render_settings_ {render_settings},
        state_ {state},
        zoom_clock_ {zoom_clock} {}

    using sender_concept = stdexec::sender_t;

    template<typename Env>
    static auto get_completion_signatures(const Env&) {
        return stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_error_t(std::exception_ptr),
                                              stdexec::set_stopped_t()> {};
    }

    template<typename Receiver>
    struct OperationState {
        Receiver receiver_;
        sf::RenderWindow& window_;
        RenderSettings render_settings_;
        AppState& state_;
        sf::Clock& zoom_clock_;

        static constexpr float ZOOM_INTERVAL_MS = 100.0f;

        template<typename R>
        explicit OperationState(R&& receiver, sf::RenderWindow& window, RenderSettings render_settings, AppState& state,
                                sf::Clock& zoom_clock):
            receiver_ {std::forward<R>(receiver)},
            window_ {window},
            render_settings_ {render_settings},
            state_ {state},
            zoom_clock_ {zoom_clock} {}

        // Реализация start в try-catch. Обработка событий изменения: AppState::need_renderer; AppState::viewport.
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
            // Проверка состояния только: нажатия ЛКМ/ПКМ и зумминг.
            HandleEvents();
            // Посылаем сигнал остановки в downstream. Модель сама корректно завершит операции и освободит ресурсы.
            if(state_.should_exit) {
                stdexec::set_stopped(std::move(receiver_));
            }
            else {
                stdexec::set_value(std::move(receiver_));
            }
        }

        void HandleEvents() {
            sf::Event event;
            state_.need_rerender = false;

            // Согласно SFML либе сигналы от переферийный устройств компьютера копятся в очереди. Сигналов может
            // быть 1 или сразу несколько.
            // Представим, что наш main pipeline благодаря repeat_effect_until запускается по кругу -
            // один кадр за раз. Поэтому HandleEvents запускается один раз за кадр. А синхронизирует частоту этих
            // кадров адаптер WaitForFPS.
            // Таким образом, сперва обновляем состояние приложения пока все сигналы не будут прочитаны. А потом,
            // исходя из обновленного состояния, делаем вызов хендлера.
            while(window_.pollEvent(event)) {
                switch(event.type) {
                    // Закрытие окна.
                    case sf::Event::EventType::Closed: {
                        state_.should_exit = true;
                        window_.close();
                        break;
                    }
                    case sf::Event::MouseButtonPressed: {
                        switch(event.mouseButton.button) {
                            // Приближение отображения фрактала.
                            case sf::Mouse::Left: {
                                state_.left_mouse_pressed = true;
                                state_.need_rerender      = true;
                                break;
                            }
                                // Отдаление отображения фрактала.
                            case sf::Mouse::Right: {
                                state_.right_mouse_pressed = true;
                                state_.need_rerender       = true;
                                break;
                            }
                            default:
                                break;
                        }
                        break;
                    }
                    case sf::Event::MouseButtonReleased: {
                        state_.left_mouse_pressed  = false;
                        state_.right_mouse_pressed = false;
                        break;
                    }
                    default:
                        break;
                }
            }

            // Возможно состояние приложения после событий обновилось, и нужно обновить viewport. Запуск хендлер
            // обновления viewport.
            if(state_.need_rerender) {
                HandleContinuousZoom();
            }
        }

        void HandleContinuousZoom() {
            // Выполняем изменение viewport только в том случае, если время между нажатиями ЛКМ/ПКМ больше минимального
            // ZOOM_INTERVAL_MS.
            if((state_.left_mouse_pressed || state_.right_mouse_pressed) &&
               zoom_clock_.getElapsedTime().asMilliseconds() >= ZOOM_INTERVAL_MS) {

                sf::Vector2i mouse_pos = sf::Mouse::getPosition(window_);

                if(mouse_pos.x >= 0 && mouse_pos.x < static_cast<int>(render_settings_.width) && mouse_pos.y >= 0 &&
                   mouse_pos.y < static_cast<int>(render_settings_.height)) {

                    ZoomToPoint(mouse_pos.x, mouse_pos.y, state_.left_mouse_pressed);
                    zoom_clock_.restart();
                }
            }
        }

        void ZoomToPoint(int pixel_x, int pixel_y, bool zoom_in, double factor = 0.8) {
            const double target_x = state_.viewport.x_min +
                                    (static_cast<double>(pixel_x) / render_settings_.width) * state_.viewport.width();
            const double target_y = state_.viewport.y_min +
                                    (static_cast<double>(pixel_y) / render_settings_.height) * state_.viewport.height();

            // Либо приближаем, либо отдаляем.
            const double zoom_factor = zoom_in ? factor : (1.0 / factor);

            const double new_width  = state_.viewport.width() * zoom_factor;
            const double new_height = state_.viewport.height() * zoom_factor;

            // Обновляем границы viewport.
            state_.viewport.x_max = target_x + new_width / 2.0;
            state_.viewport.x_min = target_x - new_width / 2.0;
            state_.viewport.y_max = target_y + new_height / 2.0;
            state_.viewport.y_min = target_y - new_height / 2.0;
        }
    };

    template<typename Receiver>
    auto connect(Receiver&& receiver) {
        return OperationState<std::decay_t<Receiver>> {std::forward<Receiver>(receiver), window_, render_settings_,
                                                       state_, zoom_clock_};
    }

    private:
    sf::RenderWindow& window_;
    RenderSettings render_settings_;
    AppState& state_;
    sf::Clock& zoom_clock_;
};
