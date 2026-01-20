#pragma once

#include <SFML/Graphics.hpp>
#include <print>
#include <ranges>
#include <stdexec/execution.hpp>

#include "types.hpp"

namespace rv = std::ranges::views;
namespace rs = std::ranges;

class SFMLRender {
    public:
    using sender_concept = stdexec::sender_t;

    template<typename Env>
    static auto get_completion_signatures(const Env&) {
        return stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_error_t(std::exception_ptr),
                                              stdexec::set_stopped_t()> {};
    }

    SFMLRender(RenderResult render_result, sf::Image& image, sf::Texture& texture, sf::Sprite& sprite,
               sf::RenderWindow& window, RenderSettings render_settings):
        render_result_(std::move(render_result)),
        image_ {image},
        texture_ {texture},
        sprite_ {sprite},
        window_ {window},
        render_settings_ {render_settings} {}

    template<typename Receiver>
    struct OperationState {
        Receiver receiver_;
        RenderResult render_result_;
        sf::Image& image_;
        sf::Texture& texture_;
        sf::Sprite& sprite_;
        sf::RenderWindow& window_;
        RenderSettings render_settings_;

        explicit OperationState(Receiver&& receiver, RenderResult render_result, sf::Image& image, sf::Texture& texture,
                                sf::Sprite& sprite, sf::RenderWindow& window, RenderSettings render_settings):
            receiver_(std::forward<Receiver>(receiver)),
            render_result_(std::move(render_result)),
            image_(image),
            texture_(texture),
            sprite_(sprite),
            window_(window),
            render_settings_(render_settings) {}

        void start() noexcept {
            try {
                run();
            }
            catch(...) {
                stdexec::set_error(std::move(receiver_), std::current_exception());
            }
        }

        private:
        void run() {  // Отрисовка фрактала на экране.
            if(!render_result_.color_data.empty()) {
                // Гарантируем, что НЕ меняем размер площади отрисовки на экране.
                assert(render_result_.color_data.size() == render_settings_.height);
                assert(render_result_.color_data.front().size() == render_settings_.width);

                window_.clear(sf::Color::Black);

                for(auto y: rv::iota(0u, render_result_.color_data.size())) {
                    for(auto x: rv::iota(0u, render_result_.color_data[y].size())) {
                        const auto& rgb = render_result_.color_data[y][x];
                        image_.setPixel(x, y, sf::Color {rgb.r, rgb.g, rgb.b});
                    }
                }

                texture_.update(image_);
                sprite_.setTexture(texture_, true);
                window_.draw(sprite_);
                window_.display();
            }
            stdexec::set_value(std::move(receiver_));
        }
    };

    template<typename Receiver>
    auto connect(Receiver&& r) {
        return OperationState<std::decay_t<Receiver>> {
            std::forward<Receiver>(r), std::move(render_result_), image_, texture_, sprite_, window_, render_settings_};
    }

    private:
    RenderResult render_result_;
    sf::Image& image_;
    sf::Texture& texture_;
    sf::Sprite& sprite_;
    sf::RenderWindow& window_;
    RenderSettings render_settings_;
};
